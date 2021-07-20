use std::mem::size_of;
use std::marker::PhantomData;
use std::io::Write;

/// Annotates a type as containing only primitive types like integers and arrays.
/// This means it's safe to abuse with raw copy, raw zero, or "type punning."
/// This is ONLY used for packed protocol header or segment objects.
pub unsafe trait RawObject: Sized {}

/// A zero length RawObject for using a Buffer when you don't want a header.
pub struct NoHeader;

unsafe impl RawObject for NoHeader {}

/// A byte array that supports safe appending of data or raw objects.
///
/// This also supports a generic header that must be a RawObject and will always be
/// placed at the beginning of the buffer. When you construct or clear() a buffer
/// space will be maintained for the header. Use NoHeader if you don't want a header.
#[derive(Clone)]
pub struct Buffer<H: RawObject, const L: usize>(usize, [u8; L], PhantomData<H>);

unsafe impl<H: RawObject, const L: usize> RawObject for Buffer<H, L> {}

impl<H: RawObject, const L: usize> Default for Buffer<H, L> {
    #[inline(always)]
    fn default() -> Self {
        Buffer(size_of::<H>(), [0_u8; L], PhantomData::default())
    }
}

impl<H: RawObject, const L: usize> Buffer<H, L> {
    #[inline(always)]
    pub fn new() -> Self {
        Self::default()
    }

    /// Get a slice containing the entire buffer in raw form including the header.
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        &self.1[0..self.0]
    }

    /// Erase contents and reset size to the size of the header.
    #[inline(always)]
    pub fn clear(&mut self) {
        self.1[0..self.0].fill(0);
        self.0 = size_of::<H>();
    }

    /// Get the length of this buffer (including header, if any).
    #[inline(always)]
    pub fn len(&self) -> usize {
        self.0
    }

    /// Get a reference to the header (in place).
    #[inline(always)]
    pub fn header(&self) -> &H {
        debug_assert!(size_of::<H>() <= L);
        unsafe { &*self.1.as_ptr().cast::<H>() }
    }

    /// Get a mutable reference to the header (in place).
    #[inline(always)]
    pub fn header_mut(&mut self) -> &mut H {
        debug_assert!(size_of::<H>() <= L);
        unsafe { &mut *self.1.as_mut_ptr().cast::<H>() }
    }

    /// Append a packed structure and call a function to initialize it in place.
    /// Anything not initialized will be zero.
    #[inline(always)]
    pub fn append_and_init_struct<T: RawObject, R, F: FnOnce(&mut T) -> R>(&mut self, initializer: F) -> std::io::Result<R> {
        let ptr = self.0;
        let end = ptr + size_of::<T>();
        if end <= L {
            self.0 = end;
            unsafe {
                Ok(initializer(&mut *self.1.as_mut_ptr().cast::<u8>().offset(ptr as isize).cast::<T>()))
            }
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append and initialize a byte array with a fixed size set at compile time.
    /// This is more efficient than setting a size at runtime as it may allow the compiler to
    /// skip some bounds checking. Any bytes not initialized will be zero.
    #[inline(always)]
    pub fn append_and_init_bytes_fixed<R, F: FnOnce(&mut [u8; N]) -> R, const N: usize>(&mut self, initializer: F) -> std::io::Result<R> {
        let ptr = self.0;
        let end = ptr + N;
        if end <= L {
            self.0 = end;
            unsafe {
                Ok(initializer(&mut *self.1.as_mut_ptr().cast::<u8>().offset(ptr as isize).cast::<[u8; N]>()))
            }
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append and initialize a slice with a size that is set at runtime.
    /// Any bytes not initialized will be zero.
    #[inline(always)]
    pub fn append_and_init_bytes<R, F: FnOnce(&mut [u8]) -> R>(&mut self, l: usize, initializer: F) -> std::io::Result<R> {
        let ptr = self.0;
        let end = ptr + l;
        if end <= L {
            self.0 = end;
            Ok(initializer(&mut self.1[ptr..end]))
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append a dynamic byte slice (copy into buffer).
    /// Use append_and_init_ functions if possible as these avoid extra copies.
    #[inline(always)]
    fn append_bytes(&mut self, buf: &[u8]) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + buf.len();
        if end <= L {
            self.0 = end;
            self.1[ptr..end].copy_from_slice(buf);
            Ok(())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append a fixed length byte array (copy into buffer).
    /// Use append_and_init_ functions if possible as these avoid extra copies.
    #[inline(always)]
    fn append_bytes_fixed<const S: usize>(&mut self, buf: &[u8; S]) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + S;
        if end <= L {
            self.0 = end;
            self.1[ptr..end].copy_from_slice(buf);
            Ok(())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append a byte
    #[inline(always)]
    fn append_u8(&mut self, i: u8) -> std::io::Result<()> {
        let ptr = self.0;
        if ptr < L {
            self.0 = ptr + 1;
            self.1[ptr] = i;
            Ok(())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append a 16-bit integer (in big-endian form)
    #[inline(always)]
    fn append_u16(&mut self, i: u16) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 2;
        if end <= L {
            self.0 = end;
            crate::util::integer_store_be_u16(i, &mut self.1[ptr..end]);
            Ok(())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append a 32-bit integer (in big-endian form)
    #[inline(always)]
    fn append_u32(&mut self, i: u32) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 4;
        if end <= L {
            self.0 = end;
            crate::util::integer_store_be_u32(i, &mut self.1[ptr..end]);
            Ok(())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Append a 64-bit integer (in big-endian form)
    #[inline(always)]
    fn append_u64(&mut self, i: u64) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 8;
        if end <= L {
            self.0 = end;
            crate::util::integer_store_be_u64(i, &mut self.1[ptr..end]);
            Ok(())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get a structure at a given position in the buffer and advance the cursor.
    #[inline(always)]
    pub fn get_struct<T: RawObject>(&self, cursor: &mut usize) -> std::io::Result<&T> {
        let ptr = *cursor;
        let end = ptr + size_of::<T>();
        if end <= self.0 {
            *cursor = end;
            unsafe {
                Ok(&*self.1.as_ptr().cast::<u8>().offset(ptr as isize).cast::<T>())
            }
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get a fixed length byte array and advance the cursor.
    /// This is slightly more efficient than reading a runtime sized byte slice.
    #[inline(always)]
    pub fn get_bytes_fixed<const S: usize>(&self, cursor: &mut usize) -> std::io::Result<&[u8; S]> {
        let ptr = *cursor;
        let end = ptr + S;
        if end <= self.0 {
            *cursor = end;
            unsafe {
                Ok(&*self.1.as_ptr().cast::<u8>().offset(ptr as isize).cast::<[u8; S]>())
            }
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get a runtime specified length byte slice and advance the cursor.
    #[inline(always)]
    pub fn get_bytes(&self, l: usize, cursor: &mut usize) -> std::io::Result<&[u8]> {
        let ptr = *cursor;
        let end = ptr + l;
        if end <= self.0 {
            *cursor = end;
            Ok(&self.1[ptr..end])
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get the next u8 and advance the cursor.
    #[inline(always)]
    pub fn get_u8(&self, cursor: &mut usize) -> std::io::Result<u8> {
        let ptr = *cursor;
        if ptr < self.0 {
            *cursor = ptr + 1;
            Ok(self.1[ptr])
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get the next u16 and advance the cursor.
    #[inline(always)]
    pub fn get_u16(&self, cursor: &mut usize) -> std::io::Result<u16> {
        let ptr = *cursor;
        let end = ptr + 2;
        if end <= self.0 {
            *cursor = end;
            Ok(crate::util::integer_load_be_u16(&self.1[ptr..end]))
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get the next u32 and advance the cursor.
    #[inline(always)]
    pub fn get_u32(&self, cursor: &mut usize) -> std::io::Result<u32> {
        let ptr = *cursor;
        let end = ptr + 4;
        if end <= self.0 {
            *cursor = end;
            Ok(crate::util::integer_load_be_u32(&self.1[ptr..end]))
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    /// Get the next u64 and advance the cursor.
    #[inline(always)]
    pub fn get_u64(&self, cursor: &mut usize) -> std::io::Result<u64> {
        let ptr = *cursor;
        let end = ptr + 8;
        if end <= self.0 {
            *cursor = end;
            Ok(crate::util::integer_load_be_u64(&self.1[ptr..end]))
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }
}

impl<H: RawObject, const L: usize> Write for Buffer<H, L> {
    #[inline(always)]
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let ptr = self.0;
        let end = ptr + buf.len();
        if end <= L {
            self.0 = end;
            self.1[ptr..end].copy_from_slice(buf);
            Ok(buf.len())
        } else {
            std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "overflow"))
        }
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use std::mem::size_of;
    use crate::vl1::buffer::NoHeader;

    #[test]
    fn object_sizing() {
        assert_eq!(size_of::<NoHeader>(), 0);
    }
}
