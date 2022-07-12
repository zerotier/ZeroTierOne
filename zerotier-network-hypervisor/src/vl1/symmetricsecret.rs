// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use zerotier_core_crypto::aes_gmac_siv::AesGmacSiv;
use zerotier_core_crypto::hash::SHA384;
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

    /// First 16 bytes of SHA384(key), used to identify sessions for ratcheting.
    pub key_hash: [u8; 16],

    /// Key for private fields in HELLO packets.
    pub hello_private_section_key: Secret<48>,

    /// Key used for HMAC extended validation on packets like HELLO.
    pub packet_hmac_key: Secret<48>,

    /// Pool of keyed AES-GMAC-SIV engines (pooled to avoid AES re-init every time).
    pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

impl SymmetricSecret {
    /// Create a new symmetric secret, deriving all sub-keys and such.
    pub fn new(key: Secret<64>) -> SymmetricSecret {
        let hello_private_section_key = zt_kbkdf_hmac_sha384(&key.0[..48], security_constants::KBKDF_KEY_USAGE_LABEL_HELLO_PRIVATE_SECTION);
        let packet_hmac_key = zt_kbkdf_hmac_sha384(&key.0[..48], security_constants::KBKDF_KEY_USAGE_LABEL_PACKET_HMAC);
        let aes_factory =
            AesGmacSivPoolFactory(zt_kbkdf_hmac_sha384(&key.0[..48], security_constants::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0).first_n(), zt_kbkdf_hmac_sha384(&key.0[..48], security_constants::KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1).first_n());
        let key_hash = SHA384::hash(key.as_bytes())[..16].try_into().unwrap();
        SymmetricSecret {
            key,
            key_hash,
            hello_private_section_key,
            packet_hmac_key,
            aes_gmac_siv: Pool::new(2, aes_factory),
        }
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
