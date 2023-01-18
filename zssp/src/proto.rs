use std::mem::size_of;

use pqc_kyber::{KYBER_CIPHERTEXTBYTES, KYBER_PUBLICKEYBYTES};
use zerotier_crypto::p384::P384_PUBLIC_KEY_SIZE;

use crate::applicationlayer::ApplicationLayer;
use crate::constants::{AES_GCM_TAG_SIZE, HEADER_SIZE, MIN_PACKET_SIZE, SESSION_ID_SIZE};
use crate::error::Error;

/// Maximum packet size for handshake packets
///
/// Packed structs are padded to this size so they can be recast to byte arrays of this size.
pub(crate) const NOISE_MAX_HANDSHAKE_PACKET_SIZE: usize = 2048;

pub(crate) const PACKET_TYPE_DATA: u8 = 0;
pub(crate) const PACKET_TYPE_ALICE_EPHEMERAL_OFFER: u8 = 1;
pub(crate) const PACKET_TYPE_BOB_EPHEMERAL_COUNTER_OFFER: u8 = 2;
pub(crate) const PACKET_TYPE_ALICE_STATIC_ACK: u8 = 3;
pub(crate) const PACKET_TYPE_ALICE_REKEY_INIT: u8 = 4;
pub(crate) const PACKET_TYPE_BOB_REKEY_ACK: u8 = 5;

pub(crate) const NOISE_XK_ALICE_EPHEMERAL_OFFER_ENCRYPTED_SECTION_START: usize = HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE;
pub(crate) const NOISE_XK_ALICE_EPHEMERAL_OFFER_ENCRYPTED_SECTION_END: usize =
    NOISE_XK_ALICE_EPHEMERAL_OFFER_ENCRYPTED_SECTION_START + SESSION_ID_SIZE + KYBER_PUBLICKEYBYTES + 8;
pub(crate) const NOISE_XK_ALICE_EPHEMERAL_OFFER_SIZE: usize = NOISE_XK_ALICE_EPHEMERAL_OFFER_ENCRYPTED_SECTION_END + AES_GCM_TAG_SIZE;

pub(crate) trait ProtocolFlatBuffer {}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct NoiseXKAliceEphemeralOffer {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub alice_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-GCM(es) encrypted section (IV is first 12 bytes of SHA384(alice_noise_e))
    pub alice_session_id: [u8; SESSION_ID_SIZE],
    pub alice_hk_public: [u8; KYBER_PUBLICKEYBYTES],
    pub salt: [u8; 8],
    // -- end encrypted section
    pub gcm_mac: [u8; 16],
    _padding: [u8; NOISE_MAX_HANDSHAKE_PACKET_SIZE - NOISE_XK_ALICE_EPHEMERAL_OFFER_SIZE],
}

pub(crate) const NOISE_XK_BOB_EPHEMERAL_COUNTER_OFFER_ENCRYPTED_SECTION_START: usize = HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE;
pub(crate) const NOISE_XK_BOB_EPHEMERAL_COUNTER_OFFER_ENCRYPTED_SECTION_END: usize =
    NOISE_XK_BOB_EPHEMERAL_COUNTER_OFFER_ENCRYPTED_SECTION_START + SESSION_ID_SIZE + KYBER_CIPHERTEXTBYTES;
pub(crate) const NOISE_XK_BOB_EPHEMERAL_COUNTER_OFFER_SIZE: usize =
    NOISE_XK_BOB_EPHEMERAL_COUNTER_OFFER_ENCRYPTED_SECTION_END + AES_GCM_TAG_SIZE;

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct NoiseXKBobEphemeralCounterOffer {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    pub bob_noise_e: [u8; P384_PUBLIC_KEY_SIZE],
    // -- start AES-GCM(es_ee) encrypted section (IV is first 12 bytes of SHA384(bob_noise_e))
    pub bob_session_id: [u8; SESSION_ID_SIZE],
    pub bob_hk_ciphertext: [u8; KYBER_CIPHERTEXTBYTES],
    // -- end encrypted sectiion
    pub gcm_mac: [u8; 16],
    _padding: [u8; NOISE_MAX_HANDSHAKE_PACKET_SIZE - NOISE_XK_BOB_EPHEMERAL_COUNTER_OFFER_SIZE],
}

/*
pub(crate) const NOISE_XK_ALICE_STATIC_ACK_FIXED_FIELDS_SIZE: usize = HEADER_SIZE + 1;

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct NoiseXKAliceStaticAck {
    pub header: [u8; HEADER_SIZE],
    pub session_protocol_version: u8,
    // -- start AES-GCM(es_ee) encrypted section (IV is first 12 bytes of SHA384(hk))
    _var_length_fields_and_padding: [u8; NOISE_MAX_HANDSHAKE_PACKET_SIZE - NOISE_XK_ALICE_STATIC_ACK_FIXED_FIELDS_SIZE],
    // alice_static_blob_length: u16,
    // alice_static_blob: [u8; ???],
    // alice_metadata_length: u16,
    // alice_metadata: [u8; ???],
    // hmac_es_ee_se_hk_psk: [u8; HMAC_SHA384_SIZE],
    // -- end encrypted section
    // pub gcm_mac: [u8; 16],
}
*/

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct AliceRekeyInit {
    pub header: [u8; HEADER_SIZE],
    // -- start AES-GCM encrypted portion (using current key)
    pub alice_e: [u8; P384_PUBLIC_KEY_SIZE],
    pub alice_hk_public: [u8; KYBER_PUBLICKEYBYTES],
    // -- end AES-GCM encrypted portion
    pub gcm_mac: [u8; 16],
}

#[allow(unused)]
#[repr(C, packed)]
pub(crate) struct BobRekeyAck {
    pub header: [u8; HEADER_SIZE],
    // -- start AES-GCM encrypted portion (using current key)
    pub bob_e: [u8; P384_PUBLIC_KEY_SIZE],
    pub bob_hk_ciphertext: [u8; KYBER_CIPHERTEXTBYTES],
    // -- end AES-GCM encrypted portion
    pub gcm_mac: [u8; 16],
}

// Annotate only these structs as being compatible with packet_buffer_as_bytes(). These structs
// are packed flat buffers containing only byte or byte array fields, making them safe to treat
// this way even on architectures that require type size aligned access.
impl ProtocolFlatBuffer for NoiseXKAliceEphemeralOffer {}
impl ProtocolFlatBuffer for NoiseXKBobEphemeralCounterOffer {}
//impl ProtocolFlatBuffer for NoiseXKAliceStaticAck {}
impl ProtocolFlatBuffer for AliceRekeyInit {}
impl ProtocolFlatBuffer for BobRekeyAck {}

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

// Down here is where the only unsafe code here lives. It's instrumented with assertions wherever
// possible and just helps us efficiently cast to/from flat buffers.

#[inline(always)]
pub(crate) fn new_packet_buffer<B: ProtocolFlatBuffer>() -> B {
    unsafe { std::mem::zeroed() }
}

#[inline(always)]
pub(crate) fn packet_buffer_as_bytes<B: ProtocolFlatBuffer>(b: &B) -> &[u8; NOISE_MAX_HANDSHAKE_PACKET_SIZE] {
    assert_eq!(size_of::<B>(), NOISE_MAX_HANDSHAKE_PACKET_SIZE);
    unsafe { &*(b as *const B).cast() }
}

#[inline(always)]
pub(crate) fn packet_buffer_as_bytes_mut<B: ProtocolFlatBuffer>(b: &mut B) -> &mut [u8; NOISE_MAX_HANDSHAKE_PACKET_SIZE] {
    assert_eq!(size_of::<B>(), NOISE_MAX_HANDSHAKE_PACKET_SIZE);
    unsafe { &mut *(b as *mut B).cast() }
}
