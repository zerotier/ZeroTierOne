// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use zerotier_core_crypto::aes_gmac_siv::AesGmacSiv;
use zerotier_core_crypto::kbkdf::*;
use zerotier_core_crypto::secret::Secret;

use crate::util::pool::{Pool, PoolFactory};
use crate::vl1::protocol::*;

/// A symmetric secret key negotiated between peers.
///
/// This contains the key and several sub-keys and ciphers keyed with sub-keys.
pub(crate) struct SymmetricSecret {
    /// Master key from which other keys are derived.
    pub key: Secret<64>,

    /// Pool of keyed AES-GMAC-SIV engines (pooled to avoid AES re-init every time).
    pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

impl SymmetricSecret {
    /// Create a new symmetric secret, deriving all sub-keys and such.
    pub fn new(key: Secret<64>) -> SymmetricSecret {
        let aes_factory = AesGmacSivPoolFactory(
            zt_kbkdf_hmac_sha384(&key.0[..48], security_constants::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0).first_n_clone(),
            zt_kbkdf_hmac_sha384(&key.0[..48], security_constants::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1).first_n_clone(),
        );
        SymmetricSecret { key, aes_gmac_siv: Pool::new(2, aes_factory) }
    }
}

pub(crate) struct AesGmacSivPoolFactory(Secret<32>, Secret<32>);

impl PoolFactory<AesGmacSiv> for AesGmacSivPoolFactory {
    #[inline(always)]
    fn create(&self) -> AesGmacSiv {
        AesGmacSiv::new(self.0.as_bytes(), self.1.as_bytes())
    }

    #[inline(always)]
    fn reset(&self, obj: &mut AesGmacSiv) {
        obj.reset();
    }
}
