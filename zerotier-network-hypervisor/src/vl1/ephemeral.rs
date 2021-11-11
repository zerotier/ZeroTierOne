/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::atomic::AtomicU32;

use zerotier_core_crypto::c25519::C25519KeyPair;
use zerotier_core_crypto::hash::SHA384_HASH_SIZE;
use zerotier_core_crypto::p521::P521KeyPair;
use zerotier_core_crypto::random::SecureRandom;
use zerotier_core_crypto::secret::Secret;
use zerotier_core_crypto::sidhp751::{SIDHPublicKeyAlice, SIDHPublicKeyBob, SIDHSecretKeyAlice, SIDHSecretKeyBob};

use crate::vl1::Address;
use crate::vl1::protocol::EPHEMERAL_SECRET_SIDH_TTL;
use crate::vl1::symmetricsecret::SymmetricSecret;

const EPHEMERAL_CIPHER_C25519: u8 = 1;
const EPHEMERAL_CIPHER_P521: u8 = 2;
const EPHEMERAL_CIPHER_SIDH_P751: u8 = 3;

#[derive(Copy, Clone)]
enum SIDHSecretKey {
    Alice(SIDHPublicKeyAlice, SIDHSecretKeyAlice),
    Bob(SIDHPublicKeyBob, SIDHSecretKeyBob)
}

impl SIDHSecretKey {
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
    pub fn generate(local_address: Address, remote_address: Address) -> SIDHSecretKey {
        let mut rng = SecureRandom::get();
        if local_address < remote_address {
            let (p, s) = zerotier_core_crypto::sidh::sidh::generate_alice_keypair(&mut rng);
            SIDHSecretKey::Alice(p, s)
        } else {
            let (p, s) = zerotier_core_crypto::sidh::sidh::generate_bob_keypair(&mut rng);
            SIDHSecretKey::Bob(p, s)
        }
    }
}

/// An ephemeral secret key negotiated to implement forward secrecy.
pub struct EphemeralSecret {
    timestamp_ticks: i64,
    c25519: C25519KeyPair,
    p521: P521KeyPair,
    sidhp751: SIDHSecretKey,
}

impl EphemeralSecret {
    /// Create a new ephemeral secret key.
    ///
    /// This contains key pairs for the asymmetric key agreement algorithms used and a
    /// timestamp used to enforce TTL.
    pub fn new(time_ticks: i64, local_address: Address, remote_address: Address) -> Self {
        EphemeralSecret {
            timestamp_ticks: time_ticks,
            c25519: C25519KeyPair::generate(true),
            p521: P521KeyPair::generate(true).expect("NIST P-521 key pair generation failed"),
            sidhp751: SIDHSecretKey::generate(local_address, remote_address),
        }
    }

    /// Create a public version of this ephemeral secret to share with our counterparty.
    pub fn public(&self) -> Vec<u8> {
        todo!()
    }
}

pub struct EphemeralSymmetricSecret {
    secret: SymmetricSecret,
    agreement_timestamp_ticks: i64,
    local_secret_timestamp_ticks: i64,
    encrypt_uses: AtomicU32,
}
