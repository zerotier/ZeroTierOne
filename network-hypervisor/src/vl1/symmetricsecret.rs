// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use zerotier_crypto::aes_gmac_siv::AesGmacSiv;
use zerotier_crypto::hash::hmac_sha384;
use zerotier_crypto::secret::Secret;

use crate::vl1::protocol::*;

use zerotier_utils::pool::{Pool, PoolFactory};

/// A symmetric secret key negotiated between peers.
///
/// This contains the key and several sub-keys and ciphers keyed with sub-keys.
pub(crate) struct SymmetricSecret {
    /// Master key from which other keys are derived.
    pub key: Secret<64>,

    /// Pool of keyed AES-GMAC-SIV engines (pooled to avoid AES re-init every time).
    pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

fn zt_kbkdf_hmac_sha384(key: &[u8], label: u8) -> Secret<48> {
    Secret(hmac_sha384(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x01, 0x80]))
}

impl SymmetricSecret {
    /// Create a new symmetric secret, deriving all sub-keys and such.
    pub fn new(key: Secret<64>) -> SymmetricSecret {
        let aes_factory = AesGmacSivPoolFactory(
            zt_kbkdf_hmac_sha384(&key.0[..48], v1::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0).first_n_clone(),
            zt_kbkdf_hmac_sha384(&key.0[..48], v1::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1).first_n_clone(),
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
