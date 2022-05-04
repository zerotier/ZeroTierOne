/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use crate::util::buffer::Buffer;

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
    #[inline(always)]
    fn to_buffer<const BL: usize>(&self) -> std::io::Result<Buffer<BL>> {
        assert!(BL >= Self::MAX_MARSHAL_SIZE);
        let mut tmp = Buffer::new();
        self.marshal(&mut tmp)?;
        Ok(tmp)
    }

    /// Unmarshal this object from a buffer.
    ///
    /// This is just a shortcut to calling unmarshal() with a zero cursor and then discarding the cursor.
    #[inline(always)]
    fn from_buffer<const BL: usize>(buf: &Buffer<BL>) -> std::io::Result<Self> {
        let mut tmp = 0;
        Self::unmarshal(buf, &mut tmp)
    }
}
