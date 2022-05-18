/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::util::buffer::Buffer;

/// Must be larger than any object we want to use with to_bytes() or from_bytes().
/// This hack can go away once Rust allows us to reference trait consts as generics.
const TEMP_BUF_SIZE: usize = 16384;

/// A super-lightweight zero-allocation serialization interface.
pub trait Marshalable: Sized {
    const MAX_MARSHAL_SIZE: usize;

    /// Write this object into a buffer.
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()>;

    /// Read this object from a buffer.
    ///
    /// The supplied cursor is advanced by the number of bytes read. If an Err is returned
    /// the value of the cursor is undefined but likely points to about where the error
    /// occurred. It may also point beyond the buffer, which would indicate an overrun error.
    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Self>;

    /// Write this marshalable entity into a buffer of the given size.
    ///
    /// This will return an Err if the buffer is too small or some other error occurs. It's just
    /// a shortcut to creating a buffer and marshaling into it.
    fn to_buffer<const BL: usize>(&self) -> std::io::Result<Buffer<BL>> {
        assert!(BL >= Self::MAX_MARSHAL_SIZE);
        let mut tmp = Buffer::new();
        self.marshal(&mut tmp)?;
        Ok(tmp)
    }

    /// Unmarshal this object from a buffer.
    ///
    /// This is just a shortcut to calling unmarshal() with a zero cursor and then discarding the cursor.
    fn from_buffer<const BL: usize>(buf: &Buffer<BL>) -> std::io::Result<Self> {
        let mut tmp = 0;
        Self::unmarshal(buf, &mut tmp)
    }

    /// Marshal and convert to a Rust vector.
    fn to_bytes(&self) -> Vec<u8> {
        assert!(Self::MAX_MARSHAL_SIZE <= TEMP_BUF_SIZE);
        let mut tmp = Buffer::<TEMP_BUF_SIZE>::new();
        assert!(self.marshal(&mut tmp).is_ok());
        tmp.as_bytes().to_vec()
    }

    /// Unmarshal from a raw slice.
    fn from_bytes(b: &[u8]) -> std::io::Result<Self> {
        if b.len() <= TEMP_BUF_SIZE {
            let mut tmp = Buffer::<TEMP_BUF_SIZE>::new_boxed();
            assert!(tmp.append_bytes(b).is_ok());
            let mut cursor = 0;
            Self::unmarshal(&tmp, &mut cursor)
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "object too large"))
        }
    }
}
