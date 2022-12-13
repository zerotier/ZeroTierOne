// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::error::Error;
use std::fmt::{Debug, Display};
use std::io::Write;

use crate::buffer::Buffer;

/// Must be larger than any object we want to use with to_bytes() or from_bytes().
/// This hack can go away once Rust allows us to reference trait consts as generics.
const TEMP_BUF_SIZE: usize = 8192;

/// A super-lightweight zero-allocation serialization interface.
pub trait Marshalable: Sized {
    const MAX_MARSHAL_SIZE: usize;

    /// Write this object into a buffer.
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), UnmarshalError>;

    /// Read this object from a buffer.
    ///
    /// The supplied cursor is advanced by the number of bytes read. If an Err is returned
    /// the value of the cursor is undefined but likely points to about where the error
    /// occurred. It may also point beyond the buffer, which would indicate an overrun error.
    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<Self, UnmarshalError>;

    /// Write this marshalable entity into a buffer of the given size.
    ///
    /// This will return an Err if the buffer is too small or some other error occurs. It's just
    /// a shortcut to creating a buffer and marshaling into it.
    #[inline]
    fn to_buffer<const BL: usize>(&self) -> Result<Buffer<BL>, UnmarshalError> {
        let mut tmp = Buffer::new();
        self.marshal(&mut tmp)?;
        Ok(tmp)
    }

    /// Unmarshal this object from a buffer.
    ///
    /// This is just a shortcut to calling unmarshal() with a zero cursor and then discarding the cursor.
    #[inline]
    fn from_buffer<const BL: usize>(buf: &Buffer<BL>) -> Result<Self, UnmarshalError> {
        let mut tmp = 0;
        Self::unmarshal(buf, &mut tmp)
    }

    /// Marshal and convert to a Rust vector.
    #[inline]
    fn to_bytes(&self) -> Vec<u8> {
        assert!(Self::MAX_MARSHAL_SIZE <= TEMP_BUF_SIZE);
        let mut tmp = Buffer::<TEMP_BUF_SIZE>::new();
        assert!(self.marshal(&mut tmp).is_ok()); // panics if TEMP_BUF_SIZE is too small
        tmp.as_bytes().to_vec()
    }

    /// Unmarshal from a raw slice.
    #[inline]
    fn from_bytes(b: &[u8]) -> Result<Self, UnmarshalError> {
        if b.len() <= TEMP_BUF_SIZE {
            let mut tmp = Buffer::<TEMP_BUF_SIZE>::new_boxed();
            assert!(tmp.append_bytes(b).is_ok());
            let mut cursor = 0;
            Self::unmarshal(&tmp, &mut cursor)
        } else {
            Err(UnmarshalError::OutOfBounds)
        }
    }

    /// Marshal a slice of marshalable objects to a concatenated byte vector.
    #[inline]
    fn marshal_multiple_to_bytes(objects: &[Self]) -> Result<Vec<u8>, UnmarshalError> {
        assert!(Self::MAX_MARSHAL_SIZE <= TEMP_BUF_SIZE);
        let mut tmp: Buffer<{ TEMP_BUF_SIZE }> = Buffer::new();
        let mut v: Vec<u8> = Vec::with_capacity(objects.len() * Self::MAX_MARSHAL_SIZE);
        for i in objects.iter() {
            i.marshal(&mut tmp)?;
            let _ = v.write_all(tmp.as_bytes());
            tmp.clear();
        }
        Ok(v)
    }

    /// Unmarshal a concatenated byte slice of marshalable objects.
    #[inline]
    fn unmarshal_multiple_from_bytes(mut bytes: &[u8]) -> Result<Vec<Self>, UnmarshalError> {
        assert!(Self::MAX_MARSHAL_SIZE <= TEMP_BUF_SIZE);
        let mut tmp: Buffer<{ TEMP_BUF_SIZE }> = Buffer::new();
        let mut v: Vec<Self> = Vec::new();
        while bytes.len() > 0 {
            let chunk_size = bytes.len().min(Self::MAX_MARSHAL_SIZE);
            if tmp.append_bytes(&bytes[..chunk_size]).is_err() {
                return Err(UnmarshalError::OutOfBounds);
            }
            let mut cursor = 0;
            v.push(Self::unmarshal(&mut tmp, &mut cursor)?);
            if cursor == 0 {
                return Err(UnmarshalError::InvalidData);
            }
            let _ = tmp.erase_first_n(cursor);
            bytes = &bytes[chunk_size..];
        }
        Ok(v)
    }

    /// Unmarshal a buffer with a byte slice of marshalable objects.
    #[inline]
    fn unmarshal_multiple<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize, eof: usize) -> Result<Vec<Self>, UnmarshalError> {
        let mut v: Vec<Self> = Vec::new();
        while *cursor < eof {
            v.push(Self::unmarshal(buf, cursor)?);
        }
        Ok(v)
    }
}

pub enum UnmarshalError {
    OutOfBounds,
    InvalidData,
    UnsupportedVersion,
    IoError(std::io::Error),
}

impl Display for UnmarshalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::OutOfBounds => f.write_str("out of bounds"),
            Self::InvalidData => f.write_str("invalid data"),
            Self::UnsupportedVersion => f.write_str("unsupported version"),
            Self::IoError(e) => f.write_str(e.to_string().as_str()),
        }
    }
}

impl Debug for UnmarshalError {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Display::fmt(self, f)
    }
}

impl Error for UnmarshalError {}

impl From<crate::buffer::OutOfBoundsError> for UnmarshalError {
    #[inline(always)]
    fn from(_: crate::buffer::OutOfBoundsError) -> Self {
        Self::OutOfBounds
    }
}

impl From<std::io::Error> for UnmarshalError {
    #[inline(always)]
    fn from(e: std::io::Error) -> Self {
        Self::IoError(e)
    }
}
