/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

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

const EPHEMERAL_PUBLIC_FLAG_HAVE_RATCHET_STATE: u8 = 0x01;

/// A set of ephemeral secret key pairs. Multiple algorithms are used.
pub(crate) struct EphemeralKeyPairSet {
    previous_ratchet_state: Option<[u8; 16]>, // Previous state of ratchet on which this agreement should build
    c25519: C25519KeyPair,                    // Hipster DJB cryptography
    p521: P521KeyPair,                        // US Federal Government cryptography
    sidhp751: Option<SIDHEphemeralKeyPair>,   // Post-quantum moon math cryptography (not used in every ratchet tick)
}

impl EphemeralKeyPairSet {
    /// Create a new ephemeral set of secret/public key pairs.
    ///
    /// This contains key pairs for the asymmetric key agreement algorithms used and a
    /// timestamp used to enforce TTL.
    pub fn new(local_address: Address, remote_address: Address, previous_ephemeral_secret: Option<&EphemeralSymmetricSecret>) -> Self {
        let (sidhp751, previous_ratchet_state) = previous_ephemeral_secret.map_or_else(|| {
            (
                Some(SIDHEphemeralKeyPair::generate(local_address, remote_address)),
                None
            )
        }, |previous_ephemeral_secret| {
            (
                if previous_ephemeral_secret.ratchet_state[0] == 0 {
                    // We include SIDH with a probability of 1/256, which for a 5 minute re-key interval
                    // means SIDH will be included about every 24 hours. SIDH is slower and is intended
                    // to guard against long term warehousing for eventual cracking with a QC, so this
                    // should be good enough for that threat model.
                    Some(SIDHEphemeralKeyPair::generate(local_address, remote_address))
                } else {
                    None
                },
                Some(previous_ephemeral_secret.ratchet_state.clone())
            )
        });
        EphemeralKeyPairSet {
            previous_ratchet_state,
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

        if self.previous_ratchet_state.is_some() {
            b.push(EPHEMERAL_PUBLIC_FLAG_HAVE_RATCHET_STATE);
            let _ = b.write_all(self.previous_ratchet_state.as_ref().unwrap());
        } else {
            b.push(0);
        }

        b.push(EphemeralKeyAgreementAlgorithm::C25519 as u8);
        let _ = varint::write(&mut b, C25519_PUBLIC_KEY_SIZE as u64);
        let _ = b.write_all(&self.c25519.public_bytes());

        let _ = self.sidhp751.as_ref().map(|sidhp751| {
            b.push(EphemeralKeyAgreementAlgorithm::SIDHP751 as u8);
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

        b.push(EphemeralKeyAgreementAlgorithm::NISTP521ECDH as u8);
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
    pub fn agree(self, time_ticks: i64, static_secret: &SymmetricSecret, previous_ephemeral_secret: Option<&EphemeralSymmetricSecret>, other_public_bytes: &[u8]) -> Option<EphemeralSymmetricSecret> {
        let (mut key, mut c25519_ratchet_count, mut sidhp751_ratchet_count, mut nistp521_ratchet_count) = previous_ephemeral_secret.map_or_else(|| {
            (
                static_secret.next_ephemeral_ratchet_key.clone(),
                0,
                0,
                0
            )
        }, |previous_ephemeral_secret| {
            (
                Secret(SHA384::hmac(&static_secret.next_ephemeral_ratchet_key.0, &previous_ephemeral_secret.secret.next_ephemeral_ratchet_key.0)),
                previous_ephemeral_secret.c25519_ratchet_count,
                previous_ephemeral_secret.sidhp751_ratchet_count,
                previous_ephemeral_secret.nistp521_ratchet_count
            )
        });

        let mut it_happened = false;
        let mut fips_compliant_exchange = false; // ends up true if last algorithm was FIPS compliant
        let mut other_public_bytes = other_public_bytes;

        // Check that the other side's ratchet state matches ours. If not the ratchet must restart.
        if other_public_bytes.is_empty() {
            return None;
        }
        if (other_public_bytes[0] & EPHEMERAL_PUBLIC_FLAG_HAVE_RATCHET_STATE) == 0 {
            if previous_ephemeral_secret.is_some() {
                return None;
            }
            other_public_bytes = &other_public_bytes[1..];
        } else {
            if other_public_bytes.len() < 17 || previous_ephemeral_secret.map_or(false, |previous_ephemeral_secret| other_public_bytes[1..17] != previous_ephemeral_secret.ratchet_state) {
                return None;
            }
            other_public_bytes = &other_public_bytes[17..];
        }

        while !other_public_bytes.is_empty() {
            let cipher = other_public_bytes[0];
            other_public_bytes = &other_public_bytes[1..];
            let key_len = varint::read(&mut other_public_bytes);
            if key_len.is_err() {
                return None;
            }
            let key_len = key_len.unwrap().0 as usize;

            match cipher.try_into() {

                Ok(EphemeralKeyAgreementAlgorithm::C25519) => {
                    if other_public_bytes.len() < C25519_PUBLIC_KEY_SIZE || key_len != C25519_PUBLIC_KEY_SIZE {
                        return None;
                    }
                    let c25519_secret = self.c25519.agree(&other_public_bytes[0..C25519_PUBLIC_KEY_SIZE]);
                    other_public_bytes = &other_public_bytes[C25519_PUBLIC_KEY_SIZE..];
                    key.0 = SHA384::hmac(&key.0, &c25519_secret.0);
                    it_happened = true;
                    fips_compliant_exchange = false;
                    c25519_ratchet_count += 1;
                },

                Ok(EphemeralKeyAgreementAlgorithm::SIDHP751) => {
                    if other_public_bytes.len() < (SIDH_P751_PUBLIC_KEY_SIZE + 1) || key_len != (SIDH_P751_PUBLIC_KEY_SIZE + 1) {
                        return None;
                    }
                    let _ = match self.sidhp751.as_ref() {
                        Some(SIDHEphemeralKeyPair::Alice(_, seck)) => {
                            if other_public_bytes[0] != 0 { // Alice can't agree with Alice
                                None
                            } else {
                                Some(Secret(seck.shared_secret(&SIDHPublicKeyBob::from_bytes(&other_public_bytes[1..(SIDH_P751_PUBLIC_KEY_SIZE + 1)]))))
                            }
                        },
                        Some(SIDHEphemeralKeyPair::Bob(_, seck)) => {
                            if other_public_bytes[0] != 1 { // Bob can't agree with Bob
                                None
                            } else {
                                Some(Secret(seck.shared_secret(&SIDHPublicKeyAlice::from_bytes(&other_public_bytes[1..(SIDH_P751_PUBLIC_KEY_SIZE + 1)]))))
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

                Ok(EphemeralKeyAgreementAlgorithm::NISTP521ECDH) => {
                    if other_public_bytes.len() < P521_PUBLIC_KEY_SIZE || key_len != P521_PUBLIC_KEY_SIZE {
                        return None;
                    }
                    let p521_public = P521PublicKey::from_bytes(&other_public_bytes[0..P521_PUBLIC_KEY_SIZE]);
                    other_public_bytes = &other_public_bytes[P521_PUBLIC_KEY_SIZE..];
                    if p521_public.is_none() {
                        return None;
                    }
                    let p521_key = self.p521.agree(p521_public.as_ref().unwrap());
                    if p521_key.is_none() {
                        return None;
                    }
                    key.0 = SHA384::hmac(&key.0, &p521_key.unwrap().0);
                    it_happened = true;
                    fips_compliant_exchange = true;
                    nistp521_ratchet_count += 1;
                },

                Err(_) => {
                    if other_public_bytes.len() < key_len {
                        return None;
                    }
                    other_public_bytes = &other_public_bytes[key_len..];
                }

            }
        }

        return if it_happened {
            let rs = zt_kbkdf_hmac_sha384(&key.0, KBKDF_KEY_USAGE_LABEL_EPHEMERAL_RATCHET_STATE_ID, 0, 0);
            Some(EphemeralSymmetricSecret {
                secret: SymmetricSecret::new(key),
                ratchet_state: (&rs.0[0..16]).try_into().unwrap(),
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
            None
        };
    }
}

/// Symmetric secret representing a step in the ephemeral keying ratchet.
pub(crate) struct EphemeralSymmetricSecret {
    /// Current ephemeral secret key.
    pub secret: SymmetricSecret,
    /// First 16 bytes of SHA384(current ephemeral secret).
    ratchet_state: [u8; 16],
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
    fips_compliant_exchange: bool,
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

    pub fn should_rekey(&self, time_ticks: i64) -> bool {
        time_ticks >= self.rekey_time || self.encrypt_uses.load(Ordering::Relaxed).max(self.decrypt_uses.load(Ordering::Relaxed)) >= EPHEMERAL_SECRET_REKEY_AFTER_USES
    }

    pub fn expired(&self, time_ticks: i64) -> bool {
        time_ticks >= self.expire_time || self.encrypt_uses.load(Ordering::Relaxed).max(self.decrypt_uses.load(Ordering::Relaxed)) >= EPHEMERAL_SECRET_REJECT_AFTER_USES
    }
}

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
    fn ephemeral_agreement() {
        let static_secret = SymmetricSecret::new(Secret([1_u8; 48]));
        let alice = EphemeralKeyPairSet::new(Address::from_u64(0xdeadbeef00).unwrap(), Address::from_u64(0xbeefdead00).unwrap(), None);
        let bob = EphemeralKeyPairSet::new(Address::from_u64(0xbeefdead00).unwrap(), Address::from_u64(0xdeadbeef00).unwrap(), None);
        let alice_public_bytes = alice.public_bytes();
        let bob_public_bytes = bob.public_bytes();
        let alice_key = alice.agree(2, &static_secret, None, bob_public_bytes.as_slice()).unwrap();
        let bob_key = bob.agree(2, &static_secret, None, alice_public_bytes.as_slice()).unwrap();
        assert_eq!(&alice_key.secret.key.0, &bob_key.secret.key.0);
        //println!("ephemeral_agreement secret: {}", zerotier_core_crypto::hex::to_string(&alice_key.secret.key.0));
    }
}
