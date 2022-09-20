// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::error::Error;
use std::fmt::{Debug, Display};

use zerotier_utils::buffer::{Buffer, OutOfBoundsError};

/// Must be larger than any object we want to use with to_bytes() or from_bytes().
/// This hack can go away once Rust allows us to reference trait consts as generics.
const TEMP_BUF_SIZE: usize = 8192;

pub enum MarshalUnmarshalError {
    OutOfBounds,
    InvalidData,
    UnsupportedVersion,
    IoError(std::io::Error),
}

impl Display for MarshalUnmarshalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::OutOfBounds => f.write_str("out of bounds"),
            Self::InvalidData => f.write_str("invalid data"),
            Self::UnsupportedVersion => f.write_str("unsupported version"),
            Self::IoError(e) => f.write_str(e.to_string().as_str()),
        }
    }
}

impl Debug for MarshalUnmarshalError {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Display::fmt(self, f)
    }
}

impl Error for MarshalUnmarshalError {}

impl From<OutOfBoundsError> for MarshalUnmarshalError {
    #[inline(always)]
    fn from(_: OutOfBoundsError) -> Self {
        Self::OutOfBounds
    }
}

impl From<std::io::Error> for MarshalUnmarshalError {
    #[inline(always)]
    fn from(e: std::io::Error) -> Self {
        Self::IoError(e)
    }
}

/// A super-lightweight zero-allocation serialization interface.
pub trait Marshalable: Sized {
    const MAX_MARSHAL_SIZE: usize;

    /// Write this object into a buffer.
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> Result<(), MarshalUnmarshalError>;

    /// Read this object from a buffer.
    ///
    /// The supplied cursor is advanced by the number of bytes read. If an Err is returned
    /// the value of the cursor is undefined but likely points to about where the error
    /// occurred. It may also point beyond the buffer, which would indicate an overrun error.
    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> Result<Self, MarshalUnmarshalError>;

    /// Write this marshalable entity into a buffer of the given size.
    ///
    /// This will return an Err if the buffer is too small or some other error occurs. It's just
    /// a shortcut to creating a buffer and marshaling into it.
    fn to_buffer<const BL: usize>(&self) -> Result<Buffer<BL>, MarshalUnmarshalError> {
        let mut tmp = Buffer::new();
        self.marshal(&mut tmp)?;
        Ok(tmp)
    }

    /// Unmarshal this object from a buffer.
    ///
    /// This is just a shortcut to calling unmarshal() with a zero cursor and then discarding the cursor.
    fn from_buffer<const BL: usize>(buf: &Buffer<BL>) -> Result<Self, MarshalUnmarshalError> {
        let mut tmp = 0;
        Self::unmarshal(buf, &mut tmp)
    }

    /// Marshal and convert to a Rust vector.
    fn to_bytes(&self) -> Vec<u8> {
        let mut tmp = Buffer::<TEMP_BUF_SIZE>::new();
        assert!(self.marshal(&mut tmp).is_ok()); // panics if TEMP_BUF_SIZE is too small
        tmp.as_bytes().to_vec()
    }

    /// Unmarshal from a raw slice.
    fn from_bytes(b: &[u8]) -> Result<Self, MarshalUnmarshalError> {
        if b.len() <= TEMP_BUF_SIZE {
            let mut tmp = Buffer::<TEMP_BUF_SIZE>::new_boxed();
            assert!(tmp.append_bytes(b).is_ok());
            let mut cursor = 0;
            Self::unmarshal(&tmp, &mut cursor)
        } else {
            Err(MarshalUnmarshalError::OutOfBounds)
        }
    }
}
