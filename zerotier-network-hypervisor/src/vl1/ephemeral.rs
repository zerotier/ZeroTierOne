/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::fmt::{Debug, Display};
use std::error::Error;
use std::sync::atomic::{AtomicU32, Ordering};
use std::io::Write;
use std::convert::TryInto;

use zerotier_core_crypto::c25519::{C25519KeyPair, C25519_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::hash::{SHA384_HASH_SIZE, SHA384};
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha384;
use zerotier_core_crypto::p521::{P521KeyPair, P521_PUBLIC_KEY_SIZE, P521PublicKey};
use zerotier_core_crypto::random::SecureRandom;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::sidhp751::{SIDHPublicKeyAlice, SIDHPublicKeyBob, SIDHSecretKeyAlice, SIDHSecretKeyBob, SIDH_P751_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::varint;

use crate::vl1::Address;
use crate::vl1::protocol::*;
use crate::vl1::symmetricsecret::SymmetricSecret;

const EPHEMERAL_PUBLIC_FLAG_HAVE_RATCHET_STATE_HMAC: u8 = 0x01;

pub const ALGORITHM_C25519: u8 = 0x01;
pub const ALGORITHM_NISTP521ECDH: u8 = 0x02;
pub const ALGORITHM_SIDHP751: u8 = 0x04;

pub enum EphemeralKeyAgreementError {
    OldPublic,
    StateMismatch,
    InvalidData,
    NoCompatibleAlgorithms
}

impl Display for EphemeralKeyAgreementError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            EphemeralKeyAgreementError::OldPublic => f.write_str("old (replayed?) public key data from remote"),
            EphemeralKeyAgreementError::StateMismatch => f.write_str("ratchet state mismatch"),
            EphemeralKeyAgreementError::InvalidData => f.write_str("invalid public key data"),
            EphemeralKeyAgreementError::NoCompatibleAlgorithms => f.write_str("no compatible algorithms in public key data")
        }
    }
}

impl Debug for EphemeralKeyAgreementError {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { <Self as Display>::fmt(self, f) }
}

impl Error for EphemeralKeyAgreementError {}

/// A set of ephemeral secret key pairs. Multiple algorithms are used.
pub(crate) struct EphemeralKeyPairSet {
    previous_ratchet_count: u64,                  // Previous ratchet count, next ratchet should be this + 1
    state_hmac: Option<[u8; 48]>,                 // HMAC of previous ratchet count, if there was a previous state
    c25519: C25519KeyPair,                        // Hipster DJB cryptography
    p521: P521KeyPair,                            // US Federal Government cryptography
    sidhp751: Option<SIDHEphemeralKeyPair>,       // Post-quantum moon math cryptography (not used in every ratchet tick)
}

impl EphemeralKeyPairSet {
    /// Create a new ephemeral set of secret/public key pairs.
    ///
    /// This contains key pairs for the asymmetric key agreement algorithms used and a
    /// timestamp used to enforce TTL.
    pub fn new(local_address: Address, remote_address: Address, previous_ephemeral_secret: Option<&EphemeralSymmetricSecret>) -> Self {
        let (sidhp751, previous_ratchet_count, state_hmac) = previous_ephemeral_secret.map_or_else(|| {
            (
                Some(SIDHEphemeralKeyPair::generate(local_address, remote_address)),
                0,
                None
            )
        }, |previous_ephemeral_secret| {
            (
                if (previous_ephemeral_secret.ratchet_count & 0xff) == 0 {
                    // We include SIDH every 256 ratchets, which for a 5 minute re-key interval
                    // means SIDH will be included about every 24 hours. SIDH is slower and is intended
                    // to guard against long term warehousing for eventual cracking with a QC, so this
                    // should be good enough for that threat model.
                    Some(SIDHEphemeralKeyPair::generate(local_address, remote_address))
                } else {
                    None
                },
                previous_ephemeral_secret.ratchet_count,
                Some(SHA384::hmac(previous_ephemeral_secret.secret.ephemeral_ratchet_state_key.as_bytes(), &previous_ephemeral_secret.ratchet_count.to_be_bytes()))
            )
        });
        EphemeralKeyPairSet {
            previous_ratchet_count,
            state_hmac,
            c25519: C25519KeyPair::generate(true),
            p521: P521KeyPair::generate(true).expect("NIST P-521 key pair generation failed"),
            sidhp751,
        }
    }

    /// Create a public version of this ephemeral secret to share with our counterparty.
    ///
    /// Note that the public key bundle is NOT self-signed or otherwise self-authenticating. It must
    /// be transmitted over an authenticated channel.
    pub fn public_bytes(&self) -> Vec<u8> {
        let mut b: Vec<u8> = Vec::with_capacity(SHA384_HASH_SIZE + 8 + C25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + SIDH_P751_PUBLIC_KEY_SIZE);

        let _ = varint::write(&mut b, self.previous_ratchet_count);

        if self.state_hmac.is_some() {
            b.push(EPHEMERAL_PUBLIC_FLAG_HAVE_RATCHET_STATE_HMAC);
            let _ = b.write_all(self.state_hmac.as_ref().unwrap());
        } else {
            b.push(0);
        }

        b.push(ALGORITHM_C25519);
        let _ = varint::write(&mut b, C25519_PUBLIC_KEY_SIZE as u64);
        let _ = b.write_all(&self.c25519.public_bytes());

        let _ = self.sidhp751.as_ref().map(|sidhp751| {
            b.push(ALGORITHM_SIDHP751);
            let _ = varint::write(&mut b, (SIDH_P751_PUBLIC_KEY_SIZE + 1) as u64);
            b.push(sidhp751.role());
            let pk = match &sidhp751 {
                SIDHEphemeralKeyPair::Alice(a, _) => a.to_bytes(),
                SIDHEphemeralKeyPair::Bob(b, _) => b.to_bytes()
            };
            let _ = b.write_all(&pk);
        });

        // FIPS note: any FIPS compliant ciphers must be last or the exchange will not be FIPS compliant. That's
        // because we chain/ratchet using KHDF and non-FIPS ciphers are considered "salt" inputs for HKDF from a
        // FIPS point of view. Final key must be HKDF(salt, a FIPS-compliant algorithm secret). There is zero
        // actual security implication to the order.

        b.push(ALGORITHM_NISTP521ECDH);
        let _ = varint::write(&mut b, P521_PUBLIC_KEY_SIZE as u64);
        let _ = b.write_all(self.p521.public_key_bytes());

        b
    }

    /// Perform ephemeral key agreement.
    ///
    /// None is returned if the public key data is malformed, no algorithms overlap, etc.
    ///
    /// Input is the previous session key. The long-lived identity key exchange key starts
    /// the ratchet sequence, or rather a key derived from it for this purpose.
    ///
    /// Since ephemeral secrets should only be used once, this consumes the object.
    pub fn agree(self, time_ticks: i64, static_secret: &SymmetricSecret, previous_ephemeral_secret: Option<&EphemeralSymmetricSecret>, other_public_bytes: &[u8]) -> Result<EphemeralSymmetricSecret, EphemeralKeyAgreementError> {
        let (mut key, mut ratchet_count, mut c25519_ratchet_count, mut sidhp751_ratchet_count, mut nistp521_ratchet_count) = previous_ephemeral_secret.map_or_else(|| {
            (
                static_secret.ephemeral_ratchet_key.clone(),
                0,
                0,
                0,
                0
            )
        }, |previous_ephemeral_secret| {
            (
                previous_ephemeral_secret.secret.ephemeral_ratchet_key.clone(),
                previous_ephemeral_secret.ratchet_count,
                previous_ephemeral_secret.c25519_ratchet_count,
                previous_ephemeral_secret.sidhp751_ratchet_count,
                previous_ephemeral_secret.nistp521_ratchet_count
            )
        });

        let mut it_happened = false; // set to true if at least one exchange occurred
        let mut fips_compliant_exchange = false; // is true in the end if the last algorithm was FIPS-compliant
        let mut other_public_bytes = other_public_bytes;

        // If the other side's ratchet counter is less than ours it means this may be a replayed
        // public key (or duplicate packet) and should be ignored. If it's greater it's a state
        // mismatch since there's no other way it could be from the future.
        let other_ratchet_count = varint::read(&mut other_public_bytes);
        if other_ratchet_count.is_err() {
            return Err(EphemeralKeyAgreementError::InvalidData);
        }
        let other_ratchet_count = other_ratchet_count.unwrap().0;
        if other_ratchet_count < ratchet_count {
            return Err(EphemeralKeyAgreementError::OldPublic);
        } else if other_ratchet_count > ratchet_count {
            return Err(EphemeralKeyAgreementError::StateMismatch);
        }

        // Now check the other side's HMAC of the ratchet state to fully verify that the ratchet
        // is aligned properly.
        if other_public_bytes.is_empty() {
            return Err(EphemeralKeyAgreementError::InvalidData);
        }
        if (other_public_bytes[0] & EPHEMERAL_PUBLIC_FLAG_HAVE_RATCHET_STATE_HMAC) != 0 {
            if other_public_bytes.len() < 49 {
                return Err(EphemeralKeyAgreementError::InvalidData);
            }
            if self.state_hmac.as_ref().map_or(true, |state_hmac| state_hmac != &other_public_bytes[1..49]) {
                return Err(EphemeralKeyAgreementError::StateMismatch);
            }
            other_public_bytes = &other_public_bytes[49..];
        } else {
            if self.state_hmac.is_some() {
                return Err(EphemeralKeyAgreementError::OldPublic);
            }
            other_public_bytes = &other_public_bytes[1..];
        }

        while !other_public_bytes.is_empty() {
            let cipher = other_public_bytes[0];
            other_public_bytes = &other_public_bytes[1..];
            let key_len = varint::read(&mut other_public_bytes);
            if key_len.is_err() {
                return Err(EphemeralKeyAgreementError::InvalidData);
            }
            let key_len = key_len.unwrap().0 as usize;

            match cipher {

                ALGORITHM_C25519 => {
                    if other_public_bytes.len() < C25519_PUBLIC_KEY_SIZE || key_len != C25519_PUBLIC_KEY_SIZE {
                        return Err(EphemeralKeyAgreementError::InvalidData);
                    }

                    let c25519_secret = self.c25519.agree(&other_public_bytes[0..C25519_PUBLIC_KEY_SIZE]);
                    other_public_bytes = &other_public_bytes[C25519_PUBLIC_KEY_SIZE..];

                    key.0 = SHA384::hmac(&key.0, &c25519_secret.0);
                    it_happened = true;
                    fips_compliant_exchange = false;
                    c25519_ratchet_count += 1;
                },

                ALGORITHM_SIDHP751 => {
                    if other_public_bytes.len() < (SIDH_P751_PUBLIC_KEY_SIZE + 1) || key_len != (SIDH_P751_PUBLIC_KEY_SIZE + 1) {
                        return Err(EphemeralKeyAgreementError::InvalidData);
                    }

                    let _ = match self.sidhp751.as_ref() {
                        Some(SIDHEphemeralKeyPair::Alice(_, seck)) => {
                            if other_public_bytes[0] != 0 { // Alice can't agree with Alice
                                Some(Secret(seck.shared_secret(&SIDHPublicKeyBob::from_bytes(&other_public_bytes[1..(SIDH_P751_PUBLIC_KEY_SIZE + 1)]))))
                            } else {
                                None
                            }
                        },
                        Some(SIDHEphemeralKeyPair::Bob(_, seck)) => {
                            if other_public_bytes[0] != 1 { // Bob can't agree with Bob
                                Some(Secret(seck.shared_secret(&SIDHPublicKeyAlice::from_bytes(&other_public_bytes[1..(SIDH_P751_PUBLIC_KEY_SIZE + 1)]))))
                            } else {
                                None
                            }
                        },
                        None => None,
                    }.map(|sidh_secret| {
                        key.0 = SHA384::hmac(&key.0, &sidh_secret.0);
                        it_happened = true;
                        fips_compliant_exchange = false;
                        sidhp751_ratchet_count += 1;
                    });
                    other_public_bytes = &other_public_bytes[(SIDH_P751_PUBLIC_KEY_SIZE + 1)..];
                },

                ALGORITHM_NISTP751ECDH => {
                    if other_public_bytes.len() < P521_PUBLIC_KEY_SIZE || key_len != P521_PUBLIC_KEY_SIZE {
                        return Err(EphemeralKeyAgreementError::InvalidData);
                    }

                    let p521_public = P521PublicKey::from_bytes(&other_public_bytes[0..P521_PUBLIC_KEY_SIZE]);
                    other_public_bytes = &other_public_bytes[P521_PUBLIC_KEY_SIZE..];
                    if p521_public.is_none() {
                        return Err(EphemeralKeyAgreementError::InvalidData);
                    }

                    let p521_key = self.p521.agree(p521_public.as_ref().unwrap());
                    if p521_key.is_none() {
                        return Err(EphemeralKeyAgreementError::InvalidData);
                    }

                    key.0 = SHA384::hmac(&key.0, &p521_key.unwrap().0);
                    it_happened = true;
                    fips_compliant_exchange = true;
                    nistp521_ratchet_count += 1;
                },

                _ => {
                    if other_public_bytes.len() < key_len {
                        return Err(EphemeralKeyAgreementError::InvalidData);
                    }
                    other_public_bytes = &other_public_bytes[key_len..];
                }

            }
        }

        return if it_happened {
            ratchet_count += 1;
            Ok(EphemeralSymmetricSecret {
                secret: SymmetricSecret::new(key),
                ratchet_count,
                rekey_time: time_ticks + EPHEMERAL_SECRET_REKEY_AFTER_TIME,
                expire_time: time_ticks + EPHEMERAL_SECRET_REJECT_AFTER_TIME,
                c25519_ratchet_count,
                sidhp751_ratchet_count,
                nistp521_ratchet_count,
                encrypt_uses: AtomicU32::new(0),
                decrypt_uses: AtomicU32::new(0),
                fips_compliant_exchange
            })
        } else {
            Err(EphemeralKeyAgreementError::NoCompatibleAlgorithms)
        };
    }
}

/// An ephemeral symmetric secret with usage timers and counters.
pub(crate) struct EphemeralSymmetricSecret {
    /// Current ephemeral secret key.
    pub secret: SymmetricSecret,
    /// Total number of ratchets that has occurred.
    ratchet_count: u64,
    /// Time at or after which we should start trying to re-key.
    rekey_time: i64,
    /// Time after which this key is no longer valid.
    expire_time: i64,
    /// Number of C25519 agreements so far in ratchet.
    c25519_ratchet_count: u64,
    /// Number of SIDH P-751 agreements so far in ratchet.
    sidhp751_ratchet_count: u64,
    /// Number of NIST P-521 ECDH agreements so far in ratchet.
    nistp521_ratchet_count: u64,
    /// Number of times this secret has been used to encrypt.
    encrypt_uses: AtomicU32,
    /// Number of times this secret has been used to decrypt.
    decrypt_uses: AtomicU32,
    /// True if most recent key exchange was NIST/FIPS compliant.
    pub fips_compliant_exchange: bool,
}

impl EphemeralSymmetricSecret {
    #[inline(always)]
    pub fn use_secret_to_encrypt(&self) -> &SymmetricSecret {
        let _ = self.encrypt_uses.fetch_add(1, Ordering::Relaxed);
        &self.secret
    }

    #[inline(always)]
    pub fn use_secret_to_decrypt(&self) -> &SymmetricSecret {
        let _ = self.decrypt_uses.fetch_add(1, Ordering::Relaxed);
        &self.secret
    }

    #[inline(always)]
    pub fn should_rekey(&self, time_ticks: i64) -> bool {
        time_ticks >= self.rekey_time || self.encrypt_uses.load(Ordering::Relaxed).max(self.decrypt_uses.load(Ordering::Relaxed)) >= EPHEMERAL_SECRET_REKEY_AFTER_USES
    }

    #[inline(always)]
    pub fn expired(&self, time_ticks: i64) -> bool {
        time_ticks >= self.expire_time || self.encrypt_uses.load(Ordering::Relaxed).max(self.decrypt_uses.load(Ordering::Relaxed)) >= EPHEMERAL_SECRET_REJECT_AFTER_USES
    }
}

/// Internal container for SIDH key pairs tracking whether this is the "alice" or "bob" side in the SIDH protocol.
enum SIDHEphemeralKeyPair {
    Alice(SIDHPublicKeyAlice, SIDHSecretKeyAlice),
    Bob(SIDHPublicKeyBob, SIDHSecretKeyBob)
}

impl SIDHEphemeralKeyPair {
    /// Generate a SIDH key pair.
    ///
    /// SIDH is weird. A key exchange must involve one participant taking a role
    /// canonically called Alice and the other wearing the Bob hat, because math.
    ///
    /// If our local address is less than the remote address, we take the Alice role.
    /// Otherwise if it's greater or equal we take the Bob role.
    ///
    /// Everything works as long as the two sides take opposite roles. There is no
    /// security implication in one side always taking one role.
    pub fn generate(local_address: Address, remote_address: Address) -> SIDHEphemeralKeyPair {
        let mut rng = SecureRandom::get();
        if local_address < remote_address {
            let (p, s) = zerotier_core_crypto::sidhp751::generate_alice_keypair(&mut rng);
            SIDHEphemeralKeyPair::Alice(p, s)
        } else {
            let (p, s) = zerotier_core_crypto::sidhp751::generate_bob_keypair(&mut rng);
            SIDHEphemeralKeyPair::Bob(p, s)
        }
    }

    /// Returns 0 if Alice, 1 if Bob.
    #[inline(always)]
    pub fn role(&self) -> u8 {
        match self {
            Self::Alice(_, _) => 0,
            Self::Bob(_, _) => 1,
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::vl1::ephemeral::EphemeralKeyPairSet;
    use crate::vl1::Address;
    use crate::vl1::symmetricsecret::SymmetricSecret;
    use zerotier_core_crypto::secret::Secret;

    #[test]
    fn ratchet() {
        let static_secret = SymmetricSecret::new(Secret([1_u8; 48]));
        let alice_address = Address::from_u64(0xdeadbeef00).unwrap();
        let bob_address = Address::from_u64(0xbeefdead00).unwrap();
        let mut alice = EphemeralKeyPairSet::new(alice_address, bob_address, None);
        let mut bob = EphemeralKeyPairSet::new(bob_address, alice_address, None);
        let mut prev_alice_key = None;
        let mut prev_bob_key = None;
        let ratchets = 16;
        for t in 1..ratchets+1 {
            let alice_public = alice.public_bytes();
            let bob_public = bob.public_bytes();
            let alice_key = alice.agree(t, &static_secret, prev_alice_key.as_ref(), bob_public.as_slice());
            let bob_key = bob.agree(t, &static_secret, prev_bob_key.as_ref(), alice_public.as_slice());
            assert!(alice_key.is_ok());
            assert!(bob_key.is_ok());
            let alice_key = alice_key.unwrap();
            let bob_key = bob_key.unwrap();
            assert_eq!(&alice_key.secret.key.0, &bob_key.secret.key.0);
            //println!("alice: c25519={} p521={} sidh={} | bob: c25519={} p521={} sidh={}", alice_key.c25519_ratchet_count, alice_key.nistp521_ratchet_count, alice_key.sidhp751_ratchet_count, bob_key.c25519_ratchet_count, bob_key.nistp521_ratchet_count, bob_key.sidhp751_ratchet_count);
            alice = EphemeralKeyPairSet::new(alice_address, bob_address, Some(&alice_key));
            bob = EphemeralKeyPairSet::new(bob_address, alice_address, Some(&bob_key));
            prev_alice_key = Some(alice_key);
            prev_bob_key = Some(bob_key);
        }
        let last_alice_key = prev_alice_key.unwrap();
        let last_bob_key = prev_bob_key.unwrap();
        assert_eq!(last_alice_key.ratchet_count, ratchets as u64);
        assert_eq!(last_bob_key.ratchet_count, ratchets as u64);
        assert_eq!(last_alice_key.c25519_ratchet_count, ratchets as u64);
        assert_eq!(last_bob_key.c25519_ratchet_count, ratchets as u64);
        assert_eq!(last_alice_key.nistp521_ratchet_count, ratchets as u64);
        assert_eq!(last_bob_key.nistp521_ratchet_count, ratchets as u64);
        assert_eq!(last_alice_key.sidhp751_ratchet_count, last_bob_key.sidhp751_ratchet_count);
        assert!(last_alice_key.sidhp751_ratchet_count >= 1);
    }
}
