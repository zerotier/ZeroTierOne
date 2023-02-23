/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::size_of;

use pqc_kyber::{KYBER_CIPHERTEXTBYTES, KYBER_PUBLICKEYBYTES};
use zerotier_crypto::hash::{HMAC_SHA384_SIZE, SHA384_HASH_SIZE};
use zerotier_crypto::p384::P384_PUBLIC_KEY_SIZE;

use crate::applicationlayer::ApplicationLayer;
use crate::constants::*;
use crate::error::Error;
use crate::sessionid::SessionId;

pub(crate) const SESSION_PROTOCOL_VERSION: u8 = 0x00;

pub(crate) const COUNTER_WINDOW_MAX_OOO: usize = 16;
pub(crate) const COUNTER_WINDOW_MAX_SKIP_AHEAD: u64 = 16777216;

pub(crate) const PACKET_TYPE_DATA: u8 = 0;
pub(crate) const PACKET_TYPE_ALICE_NOISE_XK_INIT: u8 = 1;
pub(crate) const PACKET_TYPE_BOB_NOISE_XK_ACK: u8 = 2;
pub(crate) const PACKET_TYPE_ALICE_NOISE_XK_ACK: u8 = 3;
pub(crate) const PACKET_TYPE_ALICE_REKEY_INIT: u8 = 4;
pub(crate) const PACKET_TYPE_BOB_REKEY_ACK: u8 = 5;

pub(crate) const HEADER_SIZE: usize = 16;
pub(crate) const HEADER_CHECK_ENCRYPT_START: usize = 6;
pub(crate) const HEADER_CHECK_ENCRYPT_END: usize = 22;

pub(crate) const KBKDF_KEY_USAGE_LABEL_KEX_ENCRYPTION: u8 = b'X'; // intermediate keys used in key exchanges
pub(crate) const KBKDF_KEY_USAGE_LABEL_KEX_AUTHENTICATION: u8 = b'x'; // intermediate keys used in key exchanges
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'A'; // AES-GCM in A->B direction
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'B'; // AES-GCM in B->A direction

pub(crate) const MAX_FRAGMENTS: usize = 48; // hard protocol max: 63
pub(crate) const MAX_NOISE_HANDSHAKE_FRAGMENTS: usize = 16; // enough room for p384 + ZT identity + kyber1024 + tag/hmac/etc.
pub(crate) const MAX_NOISE_HANDSHAKE_SIZE: usize = MAX_NOISE_HANDSHAKE_FRAGMENTS * MIN_TRANSPORT_MTU;

pub(crate) const AES_KEY_SIZE: usize = 32;
pub(crate) const AES_HEADER_CHECK_KEY_SIZE: usize = 16;
pub(crate) const AES_GCM_TAG_SIZE: usize = 16;
pub(crate) const AES_GCM_NONCE_SIZE: usize = 12;
pub(crate) const AES_CTR_NONCE_SIZE: usize = 12;

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct AliceNoiseXKInit {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub alice_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-CTR(es) encrypted section (IV is last 12 bytes of alice_noise_e))
    pub alice_session_id: [u8; SessionId::SIZE],
    pub alice_hk_public: [u8; KYBER_PUBLICKEYBYTES],
    pub header_check_cipher_key: [u8; AES_HEADER_CHECK_KEY_SIZE],
    // -- end encrypted section
    pub hmac_es: [u8; HMAC_SHA384_SIZE],
}

impl AliceNoiseXKInit {
    pub const ENC_START: usize = HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE;
    pub const AUTH_START: usize = Self::ENC_START + SessionId::SIZE + KYBER_PUBLICKEYBYTES + AES_HEADER_CHECK_KEY_SIZE;
    pub const SIZE: usize = Self::AUTH_START + HMAC_SHA384_SIZE;
}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct BobNoiseXKAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub bob_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-CTR(es_ee) encrypted section (IV is last 12 bytes of bob_noise_e)
    pub bob_session_id: [u8; SessionId::SIZE],
    pub bob_hk_ciphertext: [u8; KYBER_CIPHERTEXTBYTES],
    // -- end encrypted sectiion
    pub hmac_es_ee: [u8; HMAC_SHA384_SIZE],
}

impl BobNoiseXKAck {
    pub const ENC_START: usize = HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE;
    pub const AUTH_START: usize = Self::ENC_START + SessionId::SIZE + KYBER_CIPHERTEXTBYTES;
    pub const SIZE: usize = Self::AUTH_START + HMAC_SHA384_SIZE;
}

/*
#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct AliceNoiseXKAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    // -- start AES-CTR(es_ee) encrypted section (IV is first 12 bytes of SHA384(hk))
    pub alice_static_blob_length: [u8; 2],
    pub alice_static_blob: [u8; ???],
    pub alice_metadata_length: [u8; 2],
    pub alice_metadata: [u8; ???],
    // -- end encrypted section
    pub hmac_es_ee: [u8; HMAC_SHA384_SIZE],
    pub hmac_es_ee_se_hk_psk: [u8; HMAC_SHA384_SIZE],
}
*/

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct AliceRekeyInit {
    pub header: [u8; HEADER_SIZE],
    // -- start AES-GCM encrypted portion (using current key)
    pub alice_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- end AES-GCM encrypted portion
    pub gcm_mac: [u8; AES_GCM_TAG_SIZE],
}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct BobRekeyAck {
    pub header: [u8; HEADER_SIZE],
    // -- start AES-GCM encrypted portion (using current key)
    pub bob_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    pub ee_fingerprint: [u8; SHA384_HASH_SIZE],
    // -- end AES-GCM encrypted portion
    pub gcm_mac: [u8; AES_GCM_TAG_SIZE],
}

/// Assemble a series of fragments into a buffer and return the length of the assembled packet in bytes.
pub(crate) fn assemble_fragments_into<A: ApplicationLayer>(fragments: &[A::IncomingPacketBuffer], d: &mut [u8]) -> Result<usize, Error> {
    let mut l = 0;
    for i in 0..fragments.len() {
        let mut ff = fragments[i].as_ref();
        if ff.len() <= MIN_PACKET_SIZE {
            return Err(Error::InvalidPacket);
        }
        if i > 0 {
            ff = &ff[HEADER_SIZE..];
        }
        let j = l + ff.len();
        if j > d.len() {
            return Err(Error::InvalidPacket);
        }
        d[l..j].copy_from_slice(ff);
        l = j;
    }
    return Ok(l);
}

// Annotate only these structs as being compatible with packet_buffer_as_bytes(). These structs
// are packed flat buffers containing only byte or byte array fields, making them safe to treat
// this way even on architectures that require type size aligned access.
pub(crate) trait ProtocolFlatBuffer {}
impl ProtocolFlatBuffer for AliceNoiseXKInit {}
impl ProtocolFlatBuffer for BobNoiseXKAck {}
//impl ProtocolFlatBuffer for NoiseXKAliceStaticAck {}
impl ProtocolFlatBuffer for AliceRekeyInit {}
impl ProtocolFlatBuffer for BobRekeyAck {}

#[inline(always)]
pub(crate) fn byte_array_as_proto_buffer<B: ProtocolFlatBuffer>(b: &[u8]) -> Result<&B, Error> {
    if b.len() >= size_of::<B>() {
        Ok(unsafe { &*b.as_ptr().cast() })
    } else {
        Err(Error::InvalidPacket)
    }
}

#[inline(always)]
pub(crate) fn byte_array_as_proto_buffer_mut<B: ProtocolFlatBuffer>(b: &mut [u8]) -> Result<&mut B, Error> {
    if b.len() >= size_of::<B>() {
        Ok(unsafe { &mut *b.as_mut_ptr().cast() })
    } else {
        Err(Error::InvalidPacket)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn check_packed_struct_sizing() {
        assert_eq!(size_of::<AliceNoiseXKInit>(), AliceNoiseXKInit::SIZE);
        assert_eq!(size_of::<BobNoiseXKAck>(), BobNoiseXKAck::SIZE);
    }
}
