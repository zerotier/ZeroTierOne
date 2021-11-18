/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use parking_lot::Mutex;

use zerotier_core_crypto::aes_gmac_siv::{AesCtr, AesGmacSiv};
use zerotier_core_crypto::hash::SHA384_HASH_SIZE;
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha384;
use zerotier_core_crypto::secret::Secret;

use crate::util::pool::{Pool, PoolFactory};
use crate::vl1::protocol::*;

pub struct AesGmacSivPoolFactory(Secret<SHA384_HASH_SIZE>, Secret<SHA384_HASH_SIZE>);

impl PoolFactory<AesGmacSiv> for AesGmacSivPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGmacSiv { AesGmacSiv::new(&self.0.0[0..32], &self.1.0[0..32]) }

    #[inline(always)]
    fn reset(&self, obj: &mut AesGmacSiv) { obj.reset(); }
}

/// A symmetric secret key negotiated between peers.
///
/// This contains the key and several sub-keys and ciphers keyed with sub-keys.
pub struct SymmetricSecret {
    pub key: Secret<SHA384_HASH_SIZE>,
    pub packet_hmac_key: Secret<SHA384_HASH_SIZE>,
    pub next_ephemeral_ratchet_key: Secret<SHA384_HASH_SIZE>,
    pub hello_dictionary_keyed_cipher: Mutex<AesCtr>,
    pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

impl PartialEq for SymmetricSecret {
    fn eq(&self, other: &Self) -> bool { self.key.0.eq(&other.key.0) }
}

impl Eq for SymmetricSecret {}

impl SymmetricSecret {
    pub fn new(base_key: Secret<SHA384_HASH_SIZE>) -> SymmetricSecret {
        let usage_packet_hmac = zt_kbkdf_hmac_sha384(&base_key.0, KBKDF_KEY_USAGE_LABEL_PACKET_HMAC, 0, 0);
        let usage_ephemeral_ratchet = zt_kbkdf_hmac_sha384(&base_key.0, KBKDF_KEY_USAGE_LABEL_EPHEMERAL_RATCHET, 0, 0);
        let usage_hello_dictionary_key = zt_kbkdf_hmac_sha384(&base_key.0, KBKDF_KEY_USAGE_LABEL_HELLO_DICTIONARY_ENCRYPT, 0, 0);
        let aes_factory = AesGmacSivPoolFactory(
            zt_kbkdf_hmac_sha384(&base_key.0, KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0, 0, 0),
            zt_kbkdf_hmac_sha384(&base_key.0, KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1, 0, 0));
        SymmetricSecret {
            key: base_key,
            packet_hmac_key: usage_packet_hmac,
            next_ephemeral_ratchet_key: usage_ephemeral_ratchet,
            hello_dictionary_keyed_cipher: Mutex::new(AesCtr::new(&usage_hello_dictionary_key.0[0..32])),
            aes_gmac_siv: Pool::new(2, aes_factory),
        }
    }
}
