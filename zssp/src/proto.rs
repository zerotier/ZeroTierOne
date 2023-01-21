use std::mem::size_of;

use pqc_kyber::{KYBER_CIPHERTEXTBYTES, KYBER_PUBLICKEYBYTES};
use zerotier_crypto::hash::{HMAC_SHA384_SIZE, SHA384_HASH_SIZE};
use zerotier_crypto::p384::{P384_PUBLIC_KEY_SIZE, P384_SECRET_KEY_SIZE};

use crate::applicationlayer::ApplicationLayer;
use crate::constants::{AES_GCM_TAG_SIZE, HEADER_SIZE, MIN_PACKET_SIZE, SESSION_ID_SIZE};
use crate::error::Error;

pub(crate) const NOISE_MAX_HANDSHAKE_PACKET_SIZE: usize = 2048;

pub(crate) const PACKET_TYPE_DATA: u8 = 0;
pub(crate) const PACKET_TYPE_ALICE_EPHEMERAL_OFFER: u8 = 1;
pub(crate) const PACKET_TYPE_BOB_EPHEMERAL_COUNTER_OFFER: u8 = 2;
pub(crate) const PACKET_TYPE_ALICE_STATIC_ACK: u8 = 3;
pub(crate) const PACKET_TYPE_ALICE_REKEY_INIT: u8 = 4;
pub(crate) const PACKET_TYPE_BOB_REKEY_ACK: u8 = 5;

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct NoiseXKAliceEphemeralOffer {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub reserved: [u8; 8],
    pub alice_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-CTR(es) encrypted section (IV is first 12 bytes of SHA384(alice_noise_e))
    pub alice_session_id: [u8; SESSION_ID_SIZE],
    pub alice_hk_public: [u8; KYBER_PUBLICKEYBYTES],
    pub salt: [u8; 8],
    // -- end encrypted section
    pub hmac_es: [u8; HMAC_SHA384_SIZE],
}

impl NoiseXKAliceEphemeralOffer {
    pub const ENC_START: usize = HEADER_SIZE + 1 + 8 + P384_PUBLIC_KEY_SIZE;
    pub const AUTH_START: usize = size_of::<NoiseXKAliceEphemeralOffer>() - HMAC_SHA384_SIZE;
}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct NoiseXKBobEphemeralCounterOffer {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub bob_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-CTR(es_ee) encrypted section (IV is first 12 bytes of SHA384(bob_noise_e))
    pub bob_hk_ciphertext: [u8; KYBER_CIPHERTEXTBYTES],
    pub bob_note_to_self: [u8; size_of::<BobNoteToSelf>()],
    // -- end encrypted sectiion
    pub hmac_es_ee: [u8; HMAC_SHA384_SIZE],
}

impl NoiseXKBobEphemeralCounterOffer {
    pub const ENC_START: usize = HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE;
    pub const AUTH_START: usize = size_of::<NoiseXKBobEphemeralCounterOffer>() - HMAC_SHA384_SIZE;
}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct BobNoteToSelf {
    pub iv: [u8; 16],
    // -- start AES-GCM encrypted section using ephemeral secret known only to Bob
    pub timestamp: [u8; 8],
    pub alice_session_id: [u8; SESSION_ID_SIZE],
    pub bob_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    pub bob_noise_e_secret: [u8; P384_SECRET_KEY_SIZE],
    pub hk: [u8; 32],
    pub noise_es_ee: [u8; 64],
    // -- end encrypted sectiion
    pub gcm_mac: [u8; AES_GCM_TAG_SIZE],
}

impl BobNoteToSelf {
    pub const IV_SIZE: usize = 16;
    pub const ENC_START: usize = Self::IV_SIZE;
    pub const AUTH_START: usize = size_of::<BobNoteToSelf>() - AES_GCM_TAG_SIZE;
}

// These are variable length and so they're only here for documentation purposes.

pub(crate) const NOISE_XK_ALICE_STATIC_ACK_BOB_NOTE_TO_SELF_START: usize = HEADER_SIZE + 1;
pub(crate) const NOISE_XK_ALICE_STATIC_ACK_BOB_NOTE_TO_SELF_END: usize = HEADER_SIZE + 1 + size_of::<BobNoteToSelf>();
pub(crate) const NOISE_XK_ALICE_STATIC_ACK_ENCRYPTED_SECTION_START: usize = NOISE_XK_ALICE_STATIC_ACK_BOB_NOTE_TO_SELF_END;
pub(crate) const NOISE_XK_ALICE_STATIC_ACK_MIN_SIZE: usize =
    NOISE_XK_ALICE_STATIC_ACK_BOB_NOTE_TO_SELF_END + 2 + 2 + HMAC_SHA384_SIZE + HMAC_SHA384_SIZE;

/*
#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct NoiseXKAliceStaticAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub bob_note_to_self: [u8; size_of::<BobNoteToSelf>()],
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
impl ProtocolFlatBuffer for NoiseXKAliceEphemeralOffer {}
impl ProtocolFlatBuffer for NoiseXKBobEphemeralCounterOffer {}
impl ProtocolFlatBuffer for BobNoteToSelf {}
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
