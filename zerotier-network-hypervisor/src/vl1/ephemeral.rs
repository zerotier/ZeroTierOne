/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::atomic::{AtomicU32, Ordering};
use std::io::Write;

use zerotier_core_crypto::c25519::{C25519KeyPair, C25519_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::hash::*;
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha512;
use zerotier_core_crypto::p384::*;
use zerotier_core_crypto::random::SecureRandom;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::sidhp751::{SIDHPublicKeyAlice, SIDHPublicKeyBob, SIDHSecretKeyAlice, SIDHSecretKeyBob, SIDH_P751_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::varint;

use crate::vl1::Address;
use crate::vl1::protocol::*;
use crate::vl1::symmetricsecret::SymmetricSecret;

pub const ALGORITHM_C25519: u8 = 0x01;
pub const ALGORITHM_NISTP384ECDH: u8 = 0x02;
pub const ALGORITHM_SIDHP751: u8 = 0x04;

/// A set of ephemeral secret key pairs and related information.
pub(crate) struct EphemeralKeyPairSet {
    next_key: Secret<64>,
    c25519: C25519KeyPair,
    p384: P384KeyPair,
    sidhp751: Option<SIDHEphemeralKeyPair>,
    previous_cumulative_algorithms: u8,
}

impl EphemeralKeyPairSet {
    /// Create a new ephemeral set of secret/public key pairs.
    ///
    /// This contains key pairs for the asymmetric key agreement algorithms used and a
    /// timestamp used to enforce TTL.
    ///
    /// The key and cumulative algorithms should come from the current in-effect ephemeral secret or
    /// should be the long-term static secret and zero if there isn't one.
    ///
    /// SIDH is slow, so it's only included in the exchange if it's never been included in any previous
    /// exchange. The threat model here is long-term data warehousing in anticipation of QC, so one SIDH
    /// per ephemeral session is probably good enough for that.
    pub fn new(local_address: Address, remote_address: Address, key: &SymmetricSecret, cumulative_algorithms: u8) -> Self {
        Self {
            next_key: zt_kbkdf_hmac_sha512(key.key.as_bytes(), KBKDF_KEY_USAGE_LABEL_EPHEMERAL_RATCHET_NEXT_KEY, 0, 0),
            c25519: C25519KeyPair::generate(),
            p384: P384KeyPair::generate(),
            sidhp751: if (cumulative_algorithms & ALGORITHM_SIDHP751) == 0 {
                Some(SIDHEphemeralKeyPair::generate(local_address, remote_address))
            } else {
                None
            },
            previous_cumulative_algorithms: cumulative_algorithms
        }
    }

    /// Create a public version of this ephemeral secret to share with our counterparty.
    ///
    /// Note that the public key bundle is NOT self-signed or otherwise self-authenticating. It must
    /// be transmitted over an authenticated channel.
    pub fn public_bytes(&self) -> Vec<u8> {
        let mut b: Vec<u8> = Vec::with_capacity(C25519_PUBLIC_KEY_SIZE + P384_PUBLIC_KEY_SIZE + SIDH_P751_PUBLIC_KEY_SIZE + 8);

        b.push(ALGORITHM_C25519);
        let _ = varint::write(&mut b, C25519_PUBLIC_KEY_SIZE as u64);
        let _ = b.write_all(&self.c25519.public_bytes());

        if let Some(sidhp751) = self.sidhp751.as_ref() {
            b.push(ALGORITHM_SIDHP751);
            let _ = varint::write(&mut b, (SIDH_P751_PUBLIC_KEY_SIZE + 1) as u64);
            b.push(sidhp751.role());
            let pk = match &sidhp751 {
                SIDHEphemeralKeyPair::Alice(a, _) => a.to_bytes(),
                SIDHEphemeralKeyPair::Bob(b, _) => b.to_bytes()
            };
            let _ = b.write_all(&pk);
        }

        // FIPS note: any FIPS compliant ciphers must be last or the exchange will not be FIPS compliant. That's
        // because we chain/ratchet using KHDF and non-FIPS ciphers are considered "salt" inputs for HKDF from a
        // FIPS point of view. Final key must be HKDF(salt, a FIPS-compliant algorithm secret). There is zero
        // actual security implication to the order.

        b.push(ALGORITHM_NISTP384ECDH);
        let _ = varint::write(&mut b, P384_PUBLIC_KEY_SIZE as u64);
        let _ = b.write_all(self.p384.public_key_bytes());

        b
    }

    /// Perform ephemeral key agreement.
    ///
    /// Since ephemeral secrets should only be used once, this consumes the object.
    #[allow(non_snake_case)]
    pub fn agree(self, time_ticks: i64, mut other_public_bytes: &[u8]) -> Option<EphemeralSymmetricSecret> {
        let mut algorithms_successful: u8 = 0;
        let mut fips_compliant_exchange = false;
        let mut key: Secret<64> = self.next_key.clone();

        while !other_public_bytes.is_empty() {
            let algorithms = other_public_bytes[0];
            other_public_bytes = &other_public_bytes[1..];
            let key_len = varint::read(&mut other_public_bytes);
            if key_len.is_err() {
                return None;
            }
            let key_len = key_len.unwrap().0 as usize;

            if algorithms == ALGORITHM_C25519 {
                if other_public_bytes.len() < C25519_PUBLIC_KEY_SIZE || key_len != C25519_PUBLIC_KEY_SIZE {
                    return None;
                }

                let c25519_secret = self.c25519.agree(&other_public_bytes[0..C25519_PUBLIC_KEY_SIZE]);
                other_public_bytes = &other_public_bytes[C25519_PUBLIC_KEY_SIZE..];

                key.0 = hmac_sha512(&key.0, &c25519_secret.0);
                algorithms_successful |= ALGORITHM_C25519;
                fips_compliant_exchange = false;
            } else if algorithms == ALGORITHM_SIDHP751 {
                if other_public_bytes.len() < (SIDH_P751_PUBLIC_KEY_SIZE + 1) || key_len != (SIDH_P751_PUBLIC_KEY_SIZE + 1) {
                    return None;
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
                    key.0 = hmac_sha512(&key.0, &sidh_secret.0);
                    algorithms_successful |= ALGORITHM_SIDHP751;
                    fips_compliant_exchange = false;
                });
                other_public_bytes = &other_public_bytes[(SIDH_P751_PUBLIC_KEY_SIZE + 1)..];
            } else if algorithms == ALGORITHM_NISTP384ECDH {
                if other_public_bytes.len() < P384_PUBLIC_KEY_SIZE || key_len != P384_PUBLIC_KEY_SIZE {
                    return None;
                }

                let p384_public = P384PublicKey::from_bytes(&other_public_bytes[0..P384_PUBLIC_KEY_SIZE]);
                other_public_bytes = &other_public_bytes[P384_PUBLIC_KEY_SIZE..];
                if p384_public.is_none() {
                    return None;
                }

                let _ = self.p384.agree(p384_public.as_ref().unwrap()).map(|p384_key| {
                    key.0 = hmac_sha512(&key.0, &p384_key.0);
                    algorithms_successful |= ALGORITHM_NISTP384ECDH;
                    fips_compliant_exchange = true;
                });
            } else {
                if other_public_bytes.len() < key_len {
                    return None;
                }
                other_public_bytes = &other_public_bytes[key_len..];
            }
        }

        return if algorithms_successful != 0 {
            Some(EphemeralSymmetricSecret {
                secret: SymmetricSecret::new(key),
                rekey_time: time_ticks + EPHEMERAL_SECRET_REKEY_AFTER_TIME,
                expire_time: time_ticks + EPHEMERAL_SECRET_REJECT_AFTER_TIME,
                encrypt_uses: AtomicU32::new(0),
                decrypt_uses: AtomicU32::new(0),
                algorithms: algorithms_successful,
                cumulative_algorithms: algorithms_successful | self.previous_cumulative_algorithms,
                fips_compliant_exchange
            })
        } else {
            None
        };
    }
}

/// An ephemeral symmetric secret with usage timers and counters.
pub(crate) struct EphemeralSymmetricSecret {
    /// Current ephemeral secret key.
    pub secret: SymmetricSecret,
    /// Time at or after which we should start trying to re-key.
    pub rekey_time: i64,
    /// Time after which this key is no longer valid.
    pub expire_time: i64,
    /// Number of times this secret has been used to encrypt.
    pub encrypt_uses: AtomicU32,
    /// Number of times this secret has been used to decrypt.
    pub decrypt_uses: AtomicU32,
    /// Algorithms used in this exchange (bit mask).
    pub algorithms: u8,
    /// Cumulative algorithm mask including previous exchange algorithms.
    pub cumulative_algorithms: u8,
    /// True if most recent key exchange was NIST/FIPS compliant.
    pub fips_compliant_exchange: bool,
}

impl EphemeralSymmetricSecret {
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
        let static_secret = SymmetricSecret::new(Secret([1_u8; 64]));
        let alice_address = Address::from_u64(0xdeadbeef00).unwrap();
        let bob_address = Address::from_u64(0xbeefdead00).unwrap();
        let mut alice = EphemeralKeyPairSet::new(alice_address, bob_address, &static_secret, 0);
        let mut bob = EphemeralKeyPairSet::new(bob_address, alice_address, &static_secret, 0);
        let ratchets = 16;
        let mut alice_cumulative_algorithms: u8 = 0;
        let mut bob_cumulative_algorithms: u8 = 0;
        for t in 1..ratchets+1 {
            let alice_public = alice.public_bytes();
            let bob_public = bob.public_bytes();
            let alice_key = alice.agree(t, bob_public.as_slice());
            let bob_key = bob.agree(t, alice_public.as_slice());
            assert!(alice_key.is_some());
            assert!(bob_key.is_some());
            let alice_key = alice_key.unwrap();
            let bob_key = bob_key.unwrap();
            alice_cumulative_algorithms |= alice_key.cumulative_algorithms;
            bob_cumulative_algorithms |= bob_key.cumulative_algorithms;
            assert_eq!(&alice_key.secret.key.0, &bob_key.secret.key.0);
            alice = EphemeralKeyPairSet::new(alice_address, bob_address, &alice_key.secret, alice_key.cumulative_algorithms);
            bob = EphemeralKeyPairSet::new(bob_address, alice_address, &alice_key.secret, alice_key.cumulative_algorithms);
        }
        assert_ne!(alice_cumulative_algorithms, 0);
        assert_ne!(bob_cumulative_algorithms, 0);
    }
}
