/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use zerotier_core_crypto::aes_gmac_siv::AesGmacSiv;
use zerotier_core_crypto::kbkdf::*;
use zerotier_core_crypto::secret::Secret;

use crate::util::pool::{Pool, PoolFactory};
use crate::vl1::protocol::*;

/// Pool of reusable AES-GMAC-SIV instances.
pub(crate) struct AesGmacSivPoolFactory(Secret<48>, Secret<48>);

impl PoolFactory<AesGmacSiv> for AesGmacSivPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGmacSiv { AesGmacSiv::new(&self.0.0[0..32], &self.1.0[0..32]) }

    #[inline(always)]
    fn reset(&self, obj: &mut AesGmacSiv) { obj.reset(); }
}

/// A symmetric secret key negotiated between peers.
///
/// This contains the key and several sub-keys and ciphers keyed with sub-keys.
pub(crate) struct SymmetricSecret {
    /// The root shared symmetric secret from which other keys are derived.
    pub key: Secret<64>,

    /// Key for adding an HMAC to packets e.g. in v2+ HELLO.
    pub packet_hmac_key: Secret<64>,

    /// A pool of reusable keyed and initialized AES-GMAC-SIV ciphers.
    pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

impl PartialEq for SymmetricSecret {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool { self.key.0.eq(&other.key.0) }
}

impl Eq for SymmetricSecret {}

impl SymmetricSecret {
    /// Create a new symmetric secret, deriving all sub-keys and such.
    pub fn new(base_key: Secret<64>) -> SymmetricSecret {
        let usage_packet_hmac = zt_kbkdf_hmac_sha512(&base_key.0, KBKDF_KEY_USAGE_LABEL_PACKET_HMAC, 0, 0);
        let aes_factory = AesGmacSivPoolFactory(
            zt_kbkdf_hmac_sha384(&base_key.0[0..48], KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0, 0, 0),
            zt_kbkdf_hmac_sha384(&base_key.0[0..48], KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1, 0, 0));
        SymmetricSecret {
            key: base_key,
            packet_hmac_key: usage_packet_hmac,
            aes_gmac_siv: Pool::new(2, aes_factory),
        }
    }
}
