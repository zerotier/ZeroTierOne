/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::atomic::{AtomicUsize, Ordering};

use zerotier_core_crypto::aes_gmac_siv::AesGmacSiv;
use zerotier_core_crypto::kbkdf::*;
use zerotier_core_crypto::secret::Secret;

use crate::util::pool::{Pool, PoolFactory};
use crate::vl1::protocol::*;

/// Pool of reusable AES-GMAC-SIV instances.
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

/// A symmetric secret key negotiated between peers.
///
/// This contains the key and several sub-keys and ciphers keyed with sub-keys.
pub(crate) struct SymmetricSecret {
    /// Master key from which other keys are derived.
    pub key: Secret<64>,

    /// Key for private fields in HELLO packets.
    pub hello_private_section_key: Secret<48>,

    /// Key used for HMAC extended validation on packets like HELLO.
    pub packet_hmac_key: Secret<64>,

    /// Key used with ephemeral keying/re-keying.
    pub ephemeral_ratchet_key: Secret<64>,

    /// Pool of keyed AES-GMAC-SIV engines (pooled to avoid AES re-init every time).
    pub aes_gmac_siv: Pool<AesGmacSiv, AesGmacSivPoolFactory>,
}

impl PartialEq for SymmetricSecret {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.key == other.key
    }
}

impl Eq for SymmetricSecret {}

impl SymmetricSecret {
    /// Create a new symmetric secret, deriving all sub-keys and such.
    pub fn new(key: Secret<64>) -> SymmetricSecret {
        let hello_private_section_key = zt_kbkdf_hmac_sha384(&key.0, KBKDF_KEY_USAGE_LABEL_HELLO_PRIVATE_SECTION, 0, 0);
        let packet_hmac_key = zt_kbkdf_hmac_sha512(&key.0, KBKDF_KEY_USAGE_LABEL_PACKET_HMAC, 0, 0);
        let ephemeral_ratchet_key = zt_kbkdf_hmac_sha512(&key.0, KBKDF_KEY_USAGE_LABEL_EPHEMERAL_RATCHET_KEY, 0, 0);
        let aes_factory = AesGmacSivPoolFactory(zt_kbkdf_hmac_sha384(&key.0[0..48], KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K0, 0, 0).first_n(), zt_kbkdf_hmac_sha384(&key.0[0..48], KBKDF_KEY_USAGE_LABEL_AES_GMAC_SIV_K1, 0, 0).first_n());
        SymmetricSecret {
            key,
            hello_private_section_key,
            packet_hmac_key,
            ephemeral_ratchet_key,
            aes_gmac_siv: Pool::new(2, aes_factory),
        }
    }
}

/// An ephemeral symmetric secret with usage timers and counters.
pub(crate) struct EphemeralSymmetricSecret {
    pub id: [u8; 16], // first 16 bytes of SHA384 of symmetric secret
    pub secret: SymmetricSecret,
    pub rekey_time: i64,
    pub expire_time: i64,
    pub ratchet_count: u64,
    pub encrypt_uses: AtomicUsize,
    pub decrypt_uses: AtomicUsize,
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
