/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::atomic::AtomicU32;
use std::io::Write;
use std::convert::TryInto;

use zerotier_core_crypto::c25519::{C25519KeyPair, C25519_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::hash::{SHA384_HASH_SIZE, SHA384};
use zerotier_core_crypto::p521::{P521KeyPair, P521_PUBLIC_KEY_SIZE, P521PublicKey};
use zerotier_core_crypto::random::SecureRandom;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::sidhp751::{SIDHPublicKeyAlice, SIDHPublicKeyBob, SIDHSecretKeyAlice, SIDHSecretKeyBob, SIDH_P751_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::varint;

use crate::vl1::Address;
use crate::vl1::protocol::EphemeralKeyAgreementAlgorithm;
use crate::vl1::symmetricsecret::SymmetricSecret;

#[derive(Copy, Clone)]
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

/// An ephemeral secret key negotiated to implement forward secrecy.
pub struct EphemeralSecret {
    timestamp_ticks: i64,
    ratchet_count: u64,
    c25519: C25519KeyPair,
    p521: P521KeyPair,
    sidhp751: Option<SIDHEphemeralKeyPair>,
}

impl EphemeralSecret {
    /// Create a new ephemeral secret key.
    ///
    /// This contains key pairs for the asymmetric key agreement algorithms used and a
    /// timestamp used to enforce TTL.
    ///
    /// SIDH is much slower than Curve25519 and NIST P-521, so it's only included every
    /// 256 clicks of the ratchet. The point of SIDH is forward secrecy out to the age
    /// of quantum computing in case someone is warehousing traffic today to analyze
    /// tomorrow. An attacker from 5-15 years from now will not be able to time travel
    /// back in time and steal an ephemeral SIDH secret key with a side channel attack.
    pub fn new(time_ticks: i64, local_address: Address, remote_address: Address, previous_ephemeral_secret: Option<&EphemeralSymmetricSecret>) -> Self {
        let ratchet_count = previous_ephemeral_secret.map_or(0_u64, |previous_ephemeral_secret| previous_ephemeral_secret.next_ratchet_count);
        EphemeralSecret {
            timestamp_ticks: time_ticks,
            ratchet_count,
            c25519: C25519KeyPair::generate(true),
            p521: P521KeyPair::generate(true).expect("NIST P-521 key pair generation failed"),
            sidhp751: if (ratchet_count % 256) == 0 {
                Some(SIDHEphemeralKeyPair::generate(local_address, remote_address))
            } else {
                None
            },
        }
    }

    /// Create a public version of this ephemeral secret to share with our counterparty.
    pub fn public_bytes(&self) -> Vec<u8> {
        let mut b: Vec<u8> = Vec::with_capacity(8 + C25519_PUBLIC_KEY_SIZE + P521_PUBLIC_KEY_SIZE + SIDH_P751_PUBLIC_KEY_SIZE);

        b.push(EphemeralKeyAgreementAlgorithm::C25519 as u8);
        let _ = varint::write(&mut b, C25519_PUBLIC_KEY_SIZE as u64);
        let _ = b.write_all(&self.c25519.public_bytes());

        let _ = self.sidhp751.map(|sidhp751| {
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
        // FIPS point of view. Final key must be HKDF(salt, FIPS-compliant algorithm secret). Order has no actual
        // implication for security.

        b.push(EphemeralKeyAgreementAlgorithm::NistP521ECDH as u8);
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
        let mut key = previous_ephemeral_secret.map_or_else(|| {
            static_secret.next_ephemeral_ratchet_key.clone()
        }, |previous_ephemeral_secret| {
            Secret(SHA384::hmac(&static_secret.next_ephemeral_ratchet_key.0, &previous_ephemeral_secret.secret.next_ephemeral_ratchet_key.0))
        });

        let mut algs: Vec<EphemeralKeyAgreementAlgorithm> = Vec::with_capacity(3);
        let mut other_public_bytes = other_public_bytes;
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
                    algs.push(EphemeralKeyAgreementAlgorithm::C25519);
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
                        algs.push(EphemeralKeyAgreementAlgorithm::SIDHP751);
                    });
                    other_public_bytes = &other_public_bytes[(SIDH_P751_PUBLIC_KEY_SIZE + 1)..];
                },

                Ok(EphemeralKeyAgreementAlgorithm::NistP521ECDH) => {
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
                    algs.push(EphemeralKeyAgreementAlgorithm::NistP521ECDH);
                },

                Err(_) => {
                    if other_public_bytes.len() < key_len {
                        return None;
                    }
                    other_public_bytes = &other_public_bytes[key_len..];
                }

            }
        }

        return if !algs.is_empty() {
            Some(EphemeralSymmetricSecret {
                secret: SymmetricSecret::new(key),
                agreement_algorithms: algs,
                agreement_timestamp_ticks: time_ticks,
                local_secret_timestamp_ticks: self.timestamp_ticks,
                next_ratchet_count: self.ratchet_count + 1,
                encrypt_uses: AtomicU32::new(0),
                decrypt_uses: AtomicU32::new(0)
            })
        } else {
            None
        };
    }
}

pub struct EphemeralSymmetricSecret {
    secret: SymmetricSecret,
    agreement_algorithms: Vec<EphemeralKeyAgreementAlgorithm>,
    agreement_timestamp_ticks: i64,
    local_secret_timestamp_ticks: i64,
    next_ratchet_count: u64,
    encrypt_uses: AtomicU32,
    decrypt_uses: AtomicU32,
}

impl EphemeralSymmetricSecret {
    #[inline(always)]
    pub fn use_secret_to_encrypt(&self) -> &SymmetricSecret {
        let _ = self.encrypt_uses.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        &self.secret
    }

    #[inline(always)]
    pub fn use_secret_to_decrypt(&self) -> &SymmetricSecret {
        let _ = self.decrypt_uses.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        &self.secret
    }

    pub fn is_fips_compliant(&self) -> bool {
        self.agreement_algorithms.last().map_or(false, |alg| alg.is_fips_compliant())
    }
}

#[cfg(test)]
mod tests {
    use crate::vl1::ephemeral::EphemeralSecret;
    use crate::vl1::Address;
    use crate::vl1::symmetricsecret::SymmetricSecret;
    use zerotier_core_crypto::secret::Secret;

    #[test]
    fn ephemeral_agreement() {
        let static_secret = SymmetricSecret::new(Secret([1_u8; 48]));
        let alice = EphemeralSecret::new(1, Address::from_u64(0xdeadbeef00).unwrap(), Address::from_u64(0xbeefdead00).unwrap(), None);
        let bob = EphemeralSecret::new(1, Address::from_u64(0xbeefdead00).unwrap(), Address::from_u64(0xdeadbeef00).unwrap(), None);
        let alice_public_bytes = alice.public_bytes();
        let bob_public_bytes = bob.public_bytes();
        let alice_key = alice.agree(2, &static_secret, None, bob_public_bytes.as_slice()).unwrap();
        let bob_key = bob.agree(2, &static_secret, None, alice_public_bytes.as_slice()).unwrap();
        assert_eq!(&alice_key.secret.key.0, &bob_key.secret.key.0);
        //println!("ephemeral_agreement secret: {}", zerotier_core_crypto::hex::to_string(&alice_key.secret.key.0));
    }
}
