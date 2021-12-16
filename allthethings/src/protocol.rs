pub(crate) const PROTOCOL_VERSION: u8 = 1;

/// No operation and no payload, sent as a heartbeat.
/// This is the only message type NOT followed by a message size varint. It's just one byte.
pub(crate) const MESSAGE_TYPE_NOP: u8 = 0;

/// An object either sent in response to a query or because it is new.
/// Payload is simply the object. The hash is not included as we compute it locally for security.
pub(crate) const MESSAGE_TYPE_OBJECT: u8 = 1;

/// Request one or more objects by identity hash with optional common prefix.
pub(crate) const MESSAGE_TYPE_GET_OBJECTS: u8 = 2;

/// HELLO message, which is all u8's and is packed and so can be parsed directly in place.
/// This message is sent at the start of any connection by both sides.
#[repr(packed)]
pub(crate) struct Hello {
    pub hello_size: u8, // technically a varint but below 0x80
    pub protocol_version: u8,
    pub flags: [u8; 4], // u32, little endian
    pub clock: [u8; 8], // u64, little endian
    pub data_set_size: [u8; 8], // u64, little endian
    pub domain_hash: [u8; 32],
    pub instance_id: [u8; 16],
    pub loopback_check_code_salt: [u8; 8],
    pub loopback_check_code: [u8; 16],
}
