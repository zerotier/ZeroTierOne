// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::io::{Read, Write};

use crate::vl1::identity::Identity;
use crate::vl1::protocol::IDENTITY_FINGERPRINT_SIZE;

use zerotier_core_crypto::varint;

/// A signed bundle of identity fingerprints of nodes through which a node might be reached (e.g. roots).
///
/// This can be sent by nodes to indicate which other nodes they wish to have used to reach them. Typically
/// these would be roots. It prevents a misbehaving or malicious root from pretending to host a node.
#[derive(Clone, PartialEq, Eq)]
pub struct CareOf {
    pub timestamp: i64,
    pub fingerprints: Vec<[u8; IDENTITY_FINGERPRINT_SIZE]>,
    pub signature: Vec<u8>,
}

impl CareOf {
    pub fn new(timestamp: i64) -> Self {
        Self { timestamp, fingerprints: Vec::new(), signature: Vec::new() }
    }

    pub fn add_care_of(&mut self, id: &Identity) {
        self.fingerprints.push(id.fingerprint);
    }

    fn to_bytes_internal(&self, include_signature: bool) -> Vec<u8> {
        let mut v: Vec<u8> = Vec::with_capacity(128 + (self.fingerprints.len() * 48));
        let _ = varint::write(&mut v, self.timestamp as u64);
        let _ = varint::write(&mut v, self.fingerprints.len() as u64);
        for f in self.fingerprints.iter() {
            let _ = v.write_all(f);
        }
        let _ = varint::write(&mut v, 0); // flags, reserved for future use
        let _ = varint::write(&mut v, 0); // extra bytes, reserved for future use
        if include_signature {
            let _ = varint::write(&mut v, self.signature.len() as u64);
            let _ = v.write_all(self.signature.as_slice());
        }
        v
    }

    #[inline(always)]
    pub fn to_bytes(&self) -> Vec<u8> {
        self.to_bytes_internal(true)
    }

    pub fn from_bytes(mut b: &[u8]) -> Option<CareOf> {
        let mut f = move || -> std::io::Result<CareOf> {
            let (timestamp, _) = varint::read(&mut b)?;
            let mut care_of = CareOf {
                timestamp: timestamp as i64,
                fingerprints: Vec::new(),
                signature: Vec::new(),
            };
            let (fingerprint_count, _) = varint::read(&mut b)?;
            for _ in 0..fingerprint_count {
                let mut tmp = [0_u8; IDENTITY_FINGERPRINT_SIZE];
                b.read_exact(&mut tmp)?;
                care_of.fingerprints.push(tmp);
            }
            let _ = varint::read(&mut b)?; // flags, currently ignored
            let (extra_bytes, _) = varint::read(&mut b)?;
            if extra_bytes > (b.len() as u64) {
                return Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, ""));
            }
            b = &b[(extra_bytes as usize)..];
            let (signature_len, _) = varint::read(&mut b)?;
            if signature_len > (b.len() as u64) {
                return Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, ""));
            }
            let _ = care_of.signature.write_all(&b[..(signature_len as usize)]);
            return Ok(care_of);
        };
        if let Ok(care_of) = f() {
            Some(care_of)
        } else {
            None
        }
    }

    /// Sort, deduplicate, and sign this care-of packet.
    ///
    /// The supplied identitiy must contain its secret keys. False is returned if there is an error.
    pub fn sign(&mut self, signer: &Identity) -> bool {
        self.fingerprints.sort_unstable();
        self.fingerprints.dedup();
        if let Some(sig) = signer.sign(self.to_bytes_internal(false).as_slice(), false) {
            self.signature = sig;
            true
        } else {
            false
        }
    }

    #[allow(unused)]
    pub fn verify(&self, signer: &Identity) -> bool {
        signer.verify(self.to_bytes_internal(false).as_slice(), self.signature.as_slice())
    }

    #[allow(unused)]
    pub fn contains(&self, id: &Identity) -> bool {
        self.fingerprints.binary_search(&id.fingerprint).is_ok()
    }
}
