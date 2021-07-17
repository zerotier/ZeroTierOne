use std::mem::size_of;
use std::marker::PhantomData;

const FAULT_BIT: usize = 1_usize << ((size_of::<usize>() * 8) - 1);
const FAULT_CLEAR_MASK: usize = !FAULT_BIT;

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
///
/// If a write overflow occurs during append operations, the operations fail silently
/// without increasing the buffer's size and an internal fault bit is set. The
/// check_overflow() method must be used before the buffer is actually complete to
/// ensure that no write overflows occurred. If this check isn't performed a buffer
/// could be used with incomplete or corrupt data, but no crash or memory errors will
/// occur.
#[derive(Clone)]
pub struct Buffer<H: RawObject, const L: usize>(usize, [u8; L], PhantomData<H>);

unsafe impl<H: RawObject, const L: usize> RawObject for Buffer<H, L> {}

impl<H: RawObject, const L: usize> Default for Buffer<H, L> {
    #[inline(always)]
    fn default() -> Self {
        assert!(size_of::<H>() <= L);
        Buffer(size_of::<H>(), [0_u8; L], PhantomData::default())
    }
}

impl<H: RawObject, const L: usize> Buffer<H, L> {
    #[inline(always)]
    pub fn new() -> Self {
        Self::default()
    }

    /// Returns true if there has been a write overflow.
    #[inline(always)]
    pub fn check_overflow(&self) -> bool {
        (self.0 & FAULT_BIT) != 0
    }

    /// Get a slice containing the entire buffer in raw form including the header.
    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        &self.1[0..(self.0 & FAULT_CLEAR_MASK)]
    }

    /// Erase contents and reset size to the size of the header.
    #[inline(always)]
    pub fn clear(&mut self) {
        self.0 = size_of::<H>();
        self.1.fill(0);
    }

    /// Get the length of this buffer (including header, if any).
    #[inline(always)]
    pub fn len(&self) -> usize {
        self.0 & FAULT_CLEAR_MASK
    }

    /// Get a reference to the header (in place).
    #[inline(always)]
    pub fn header(&self) -> &H {
        unsafe { &*self.1.as_ptr().cast::<H>() }
    }

    /// Get a mutable reference to the header (in place).
    #[inline(always)]
    pub fn header_mut(&mut self) -> &mut H {
        unsafe { &mut *self.1.as_mut_ptr().cast::<H>() }
    }

    /// Append a packed structure and initializing it in place via the supplied function.
    ///
    /// If an overflow occurs the overflow fault bit is set internally (see check_overflow())
    /// and the supplied function will never be called.
    #[inline(always)]
    pub fn append_and_init_struct<T: RawObject, F: FnOnce(&mut T)>(&mut self, initializer: F) {
        let bl = self.0;
        let s = bl + size_of::<T>();
        if s <= L {
            unsafe {
                self.0 = s;
                initializer(&mut *self.1.as_mut_ptr().cast::<u8>().offset(bl as isize).cast::<T>());
            }
        } else {
            self.0 = bl | FAULT_BIT;
        }
    }

    /// Append and initialize a byte array with a fixed size set at compile time.
    ///
    /// This is more efficient than setting a size at runtime as it may allow the compiler to
    /// skip some bounds checking.
    ///
    /// If an overflow occurs the overflow fault bit is set internally (see check_overflow())
    /// and the supplied function will never be called.
    #[inline(always)]
    pub fn append_and_init_bytes_fixed<F: FnOnce(&mut [u8; N]), const N: usize>(&mut self, initializer: F) {
        let bl = self.0;
        let s = bl + N;
        if s <= L {
            unsafe {
                let ptr = self.1.as_mut_ptr().cast::<u8>().offset(bl as isize);
                self.0 = s;
                initializer(&mut *ptr.cast::<[u8; N]>());
            }
        } else {
            self.0 = bl | FAULT_BIT;
        }
    }

    /// Append and initialize a slice with a size that is set at runtime.
    ///
    /// If an overflow occurs the overflow fault bit is set internally (see check_overflow())
    /// and the supplied function will never be called.
    #[inline(always)]
    pub fn append_and_init_bytes<F: FnOnce(&mut [u8])>(&mut self, l: usize, initializer: F) {
        let bl = self.0;
        let s = bl + l;
        if s <= L {
            self.0 = s;
            initializer(&mut self.1[bl..s]);
        } else {
            self.0 = bl | FAULT_BIT;
        }
    }

    pub fn read_payload(&self) -> Reader<H, L> {
        Reader {
            buffer: self,
            ptr: size_of::<H>(),
        }
    }
}

pub struct Reader<'a, H: RawObject, const L: usize> {
    ptr: usize,
    buffer: &'a Buffer<H, L>,
}

impl<'a, H: RawObject, const L: usize> Reader<'a, H, L> {
    pub fn read_struct<T: RawObject, R, F: FnOnce(&T, &mut Self) -> bool>(&mut self, visitor: F) -> bool {
        let rl = self.ptr;
        let s = rl + size_of::<T>();
        if s <= L {
            unsafe {
                self.ptr = s;
                visitor(&*self.buffer.1.as_ptr().cast::<u8>().offset(rl as isize).cast::<T>(), self)
            }
        } else {
            false
        }
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
