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

use crate::error::Error;
use crate::sessionid::SessionId;

/// Minimum size of a valid physical ZSSP packet of any type. Anything smaller is discarded.
pub const MIN_PACKET_SIZE: usize = HEADER_SIZE + AES_GCM_TAG_SIZE;

/// Minimum physical MTU for ZSSP to function.
pub const MIN_TRANSPORT_MTU: usize = 128;

/// Maximum combined size of static public blob and metadata.
pub const MAX_INIT_PAYLOAD_SIZE: usize = MAX_NOISE_HANDSHAKE_SIZE - ALICE_NOISE_XK_ACK_MIN_SIZE;

pub(crate) const SESSION_PROTOCOL_VERSION: u8 = 0x00;

pub(crate) const COUNTER_WINDOW_MAX_OOO: usize = 16;
pub(crate) const COUNTER_WINDOW_MAX_SKIP_AHEAD: u64 = 16777216;

pub(crate) const PACKET_TYPE_DATA: u8 = 0;
pub(crate) const PACKET_TYPE_ALICE_NOISE_XK_INIT: u8 = 1;
pub(crate) const PACKET_TYPE_BOB_NOISE_XK_ACK: u8 = 2;
pub(crate) const PACKET_TYPE_ALICE_NOISE_XK_ACK: u8 = 3;
pub(crate) const PACKET_TYPE_REKEY_INIT: u8 = 4;
pub(crate) const PACKET_TYPE_REKEY_ACK: u8 = 5;

pub(crate) const HEADER_SIZE: usize = 16;
pub(crate) const HEADER_PROTECT_ENCRYPT_START: usize = 6;
pub(crate) const HEADER_PROTECT_ENCRYPT_END: usize = 22;

pub(crate) const KBKDF_KEY_USAGE_LABEL_KEX_ENCRYPTION: u8 = b'X'; // intermediate keys used in key exchanges
pub(crate) const KBKDF_KEY_USAGE_LABEL_KEX_AUTHENTICATION: u8 = b'x'; // intermediate keys used in key exchanges
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'A'; // AES-GCM in A->B direction
pub(crate) const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'B'; // AES-GCM in B->A direction
pub(crate) const KBKDF_KEY_USAGE_LABEL_RATCHET: u8 = b'R'; // Key used in derivatin of next session key

pub(crate) const MAX_FRAGMENTS: usize = 48; // hard protocol max: 63
pub(crate) const MAX_NOISE_HANDSHAKE_FRAGMENTS: usize = 16; // enough room for p384 + ZT identity + kyber1024 + tag/hmac/etc.
pub(crate) const MAX_NOISE_HANDSHAKE_SIZE: usize = MAX_NOISE_HANDSHAKE_FRAGMENTS * MIN_TRANSPORT_MTU;

pub(crate) const BASE_KEY_SIZE: usize = 64;

pub(crate) const AES_256_KEY_SIZE: usize = 32;
pub(crate) const AES_HEADER_PROTECTION_KEY_SIZE: usize = 16;
pub(crate) const AES_GCM_TAG_SIZE: usize = 16;
pub(crate) const AES_GCM_NONCE_SIZE: usize = 12;

/// The first packet in Noise_XK exchange containing Alice's ephemeral keys, session ID, and a random
/// symmetric key to protect header fragmentation fields for this session.
#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct AliceNoiseXKInit {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub alice_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-CTR(es) encrypted section
    pub alice_session_id: [u8; SessionId::SIZE],
    pub alice_hk_public: [u8; KYBER_PUBLICKEYBYTES],
    pub header_protection_key: [u8; AES_HEADER_PROTECTION_KEY_SIZE],
    // -- end encrypted section
    pub hmac_es: [u8; HMAC_SHA384_SIZE],
}

impl AliceNoiseXKInit {
    pub const ENC_START: usize = HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE;
    pub const AUTH_START: usize = Self::ENC_START + SessionId::SIZE + KYBER_PUBLICKEYBYTES + AES_HEADER_PROTECTION_KEY_SIZE;
    pub const SIZE: usize = Self::AUTH_START + HMAC_SHA384_SIZE;
}

/// The response to AliceNoiceXKInit containing Bob's ephemeral keys.
#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct BobNoiseXKAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub bob_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-CTR(es_ee) encrypted section
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

/// Alice's final response containing her identity (she already knows Bob's) and meta-data.
/*
#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct AliceNoiseXKAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    // -- start AES-CTR(es_ee_hk) encrypted section
    pub alice_static_blob_length: [u8; 2],
    pub alice_static_blob: [u8; ???],
    pub alice_metadata_length: [u8; 2],
    pub alice_metadata: [u8; ???],
    // -- end encrypted section
    pub hmac_es_ee: [u8; HMAC_SHA384_SIZE],
    pub hmac_es_ee_se_hk_psk: [u8; HMAC_SHA384_SIZE],
}
*/

pub(crate) const ALICE_NOISE_XK_ACK_ENC_START: usize = HEADER_SIZE + 1;
pub(crate) const ALICE_NOISE_XK_ACK_AUTH_SIZE: usize = HMAC_SHA384_SIZE + HMAC_SHA384_SIZE;
pub(crate) const ALICE_NOISE_XK_ACK_MIN_SIZE: usize = ALICE_NOISE_XK_ACK_ENC_START + 2 + 2 + ALICE_NOISE_XK_ACK_AUTH_SIZE;

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct RekeyInit {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    // -- start AES-GCM encrypted portion (using current key)
    pub alice_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- end AES-GCM encrypted portion
    pub gcm_mac: [u8; AES_GCM_TAG_SIZE],
}

impl RekeyInit {
    pub const ENC_START: usize = HEADER_SIZE + 1;
    pub const AUTH_START: usize = Self::ENC_START + P384_PUBLIC_KEY_SIZE;
    pub const SIZE: usize = Self::AUTH_START + AES_GCM_TAG_SIZE;
}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct RekeyAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    // -- start AES-GCM encrypted portion (using current key)
    pub bob_e: [u8; P384_PUBLIC_KEY_SIZE],
    pub next_key_fingerprint: [u8; SHA384_HASH_SIZE],
    // -- end AES-GCM encrypted portion
    pub gcm_mac: [u8; AES_GCM_TAG_SIZE],
}

impl RekeyAck {
    pub const ENC_START: usize = HEADER_SIZE + 1;
    pub const AUTH_START: usize = Self::ENC_START + P384_PUBLIC_KEY_SIZE + SHA384_HASH_SIZE;
    pub const SIZE: usize = Self::AUTH_START + AES_GCM_TAG_SIZE;
}

// Annotate only these structs as being compatible with packet_buffer_as_bytes(). These structs
// are packed flat buffers containing only byte or byte array fields, making them safe to treat
// this way even on architectures that require type size aligned access.
pub(crate) trait ProtocolFlatBuffer {}
impl ProtocolFlatBuffer for AliceNoiseXKInit {}
impl ProtocolFlatBuffer for BobNoiseXKAck {}
//impl ProtocolFlatBuffer for NoiseXKAliceStaticAck {}
impl ProtocolFlatBuffer for RekeyInit {}
impl ProtocolFlatBuffer for RekeyAck {}

#[derive(Clone, Copy)]
#[repr(C, packed)]
struct MessageNonceCreateBuffer(u64, u32);

/// Create a 96-bit AES-GCM nonce.
///
/// The primary information that we want to be contained here is the counter and the
/// packet type. The former makes this unique and the latter's inclusion authenticates
/// it as effectively AAD. Other elements of the header are either not authenticated,
/// like fragmentation info, or their authentication is implied via key exchange like
/// the session ID.
///
/// This is also used as part of HMAC authentication for key exchange packets.
#[inline(always)]
pub(crate) fn create_message_nonce(packet_type: u8, counter: u64) -> [u8; AES_GCM_NONCE_SIZE] {
    unsafe { std::mem::transmute(MessageNonceCreateBuffer(counter.to_le(), (packet_type as u32).to_le())) }
}

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
