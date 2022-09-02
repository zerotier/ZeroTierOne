// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::io::{Read, Write};
use std::mem::{size_of, MaybeUninit};

use crate::util::pool::PoolFactory;

use zerotier_utils::varint;

/// An I/O buffer with extensions for efficiently reading and writing various objects.
///
/// WARNING: Structures can only be handled through raw read/write here if they are
/// tagged a Copy, meaning they are safe to just copy as raw memory. Care must also
/// be taken to ensure that access to them is safe on architectures that do not support
/// unaligned access. In vl1/protocol.rs this is accomplished by only using byte arrays
/// (including for integers) and accessing via things like u64::from_be_bytes() etc.
///
/// Needless to say anything with non-Copy internal members or that depends on Drop to
/// not leak resources or other higher level semantics won't work here, but Rust should
/// not let you tag that as Copy in safe code.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Buffer<const L: usize>(usize, [u8; L]);

impl<const L: usize> Default for Buffer<L> {
    #[inline(always)]
    fn default() -> Self {
        Self::new()
    }
}

// Setting attributes this way causes the 'overflow' branches to be treated as unlikely by LLVM.
#[inline(never)]
#[cold]
fn overflow_err() -> std::io::Error {
    std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "buffer overflow")
}

impl<const L: usize> Buffer<L> {
    pub const CAPACITY: usize = L;

    #[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
    #[inline(always)]
    unsafe fn read_obj_internal<T: Sized + Copy>(&self, i: usize) -> T {
        *self.1.as_ptr().add(i).cast()
    }

    #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
    #[inline(always)]
    unsafe fn read_obj_internal<T: Sized + Copy>(&self, i: usize) -> T {
        std::mem::transmute_copy(&*self.1.as_ptr().add(i).cast::<T>())
    }

    #[cfg(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64"))]
    #[inline(always)]
    unsafe fn write_obj_internal<T: Sized + Copy>(&mut self, i: usize, o: T) {
        *self.1.as_mut_ptr().add(i).cast::<T>() = o;
    }

    #[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64", target_arch = "powerpc64")))]
    #[inline(always)]
    unsafe fn write_obj_internal<T: Sized + Copy>(&mut self, i: usize, o: T) {
        std::ptr::copy_nonoverlapping((&o as *const T).cast::<u8>(), self.1.as_mut_ptr().add(i), size_of::<T>())
    }

    /// Create an empty zeroed buffer.
    #[inline(always)]
    pub fn new() -> Self {
        Self(0, [0_u8; L])
    }

    /// Create an empty zeroed buffer on the heap without intermediate stack allocation.
    /// This can be used to allocate buffers too large for the stack.
    #[inline(always)]
    pub fn new_boxed() -> Box<Self> {
        unsafe { Box::from_raw(std::alloc::alloc_zeroed(std::alloc::Layout::new::<Self>()).cast()) }
    }

    /// Create an empty buffer without internally zeroing its memory.
    ///
    /// This is unsafe because unwritten memory in the buffer will have undefined contents.
    /// This means that some of the append_X_get_mut() functions may return mutable references to
    /// undefined memory contents rather than zeroed memory.
    #[inline(always)]
    pub unsafe fn new_without_memzero() -> Self {
        Self(0, MaybeUninit::uninit().assume_init())
    }

    pub const fn capacity(&self) -> usize {
        Self::CAPACITY
    }

    pub fn from_bytes(b: &[u8]) -> std::io::Result<Self> {
        let l = b.len();
        if l <= L {
            let mut tmp = Self::new();
            tmp.0 = l;
            tmp.1[0..l].copy_from_slice(b);
            Ok(tmp)
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8] {
        &self.1[..self.0]
    }

    #[inline(always)]
    pub fn as_bytes_mut(&mut self) -> &mut [u8] {
        &mut self.1[..self.0]
    }

    #[inline(always)]
    pub fn as_ptr(&self) -> *const u8 {
        self.1.as_ptr()
    }

    #[inline(always)]
    pub fn as_mut_ptr(&mut self) -> *mut u8 {
        self.1.as_mut_ptr()
    }

    #[inline(always)]
    pub fn as_bytes_starting_at(&self, start: usize) -> std::io::Result<&[u8]> {
        if start <= self.0 {
            Ok(&self.1[start..self.0])
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn as_bytes_starting_at_mut(&mut self, start: usize) -> std::io::Result<&mut [u8]> {
        if start <= self.0 {
            Ok(&mut self.1[start..self.0])
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn as_byte_range(&self, start: usize, end: usize) -> std::io::Result<&[u8]> {
        if end <= self.0 {
            Ok(&self.1[start..end])
        } else {
            Err(overflow_err())
        }
    }

    pub fn clear(&mut self) {
        self.1[0..self.0].fill(0);
        self.0 = 0;
    }

    /// Load array into buffer.
    /// This will panic if the array is larger than L.
    pub fn set_to(&mut self, b: &[u8]) {
        let len = b.len();
        self.0 = len;
        self.1[0..len].copy_from_slice(b);
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.0
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.0 == 0
    }

    /// Set the size of this buffer's data.
    ///
    /// This will panic if the specified size is larger than L. If the size is larger
    /// than the current size uninitialized space will be zeroed.
    #[inline(always)]
    pub fn set_size(&mut self, s: usize) {
        let prev_len = self.0;
        self.0 = s;
        if s > prev_len {
            self.1[prev_len..s].fill(0);
        }
    }

    /// Get a mutable reference to the entire buffer regardless of the current 'size'.
    #[inline(always)]
    pub unsafe fn entire_buffer_mut(&mut self) -> &mut [u8; L] {
        &mut self.1
    }

    /// Set the size of the data in this buffer without checking bounds or zeroing new space.
    #[inline(always)]
    pub unsafe fn set_size_unchecked(&mut self, s: usize) {
        self.0 = s;
    }

    /// Get a byte from this buffer without checking bounds.
    #[inline(always)]
    pub unsafe fn get_unchecked(&self, i: usize) -> u8 {
        *self.1.get_unchecked(i)
    }

    /// Append a structure and return a mutable reference to its memory.
    #[inline(always)]
    pub fn append_struct_get_mut<T: Copy>(&mut self) -> std::io::Result<&mut T> {
        let ptr = self.0;
        let end = ptr + size_of::<T>();
        if end <= L {
            self.0 = end;
            Ok(unsafe { &mut *self.1.as_mut_ptr().add(ptr).cast() })
        } else {
            Err(overflow_err())
        }
    }

    /// Append a fixed size array and return a mutable reference to its memory.
    #[inline(always)]
    pub fn append_bytes_fixed_get_mut<const S: usize>(&mut self) -> std::io::Result<&mut [u8; S]> {
        let ptr = self.0;
        let end = ptr + S;
        if end <= L {
            self.0 = end;
            Ok(unsafe { &mut *self.1.as_mut_ptr().add(ptr).cast() })
        } else {
            Err(overflow_err())
        }
    }

    /// Append a runtime sized array and return a mutable reference to its memory.
    #[inline(always)]
    pub fn append_bytes_get_mut(&mut self, s: usize) -> std::io::Result<&mut [u8]> {
        let ptr = self.0;
        let end = ptr + s;
        if end <= L {
            self.0 = end;
            Ok(&mut self.1[ptr..end])
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_padding(&mut self, b: u8, count: usize) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + count;
        if end <= L {
            self.0 = end;
            self.1[ptr..end].fill(b);
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_bytes(&mut self, buf: &[u8]) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + buf.len();
        if end <= L {
            self.0 = end;
            self.1[ptr..end].copy_from_slice(buf);
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_bytes_fixed<const S: usize>(&mut self, buf: &[u8; S]) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + S;
        if end <= L {
            self.0 = end;
            self.1[ptr..end].copy_from_slice(buf);
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_varint(&mut self, i: u64) -> std::io::Result<()> {
        varint::write(self, i)
    }

    #[inline(always)]
    pub fn append_u8(&mut self, i: u8) -> std::io::Result<()> {
        let ptr = self.0;
        if ptr < L {
            self.0 = ptr + 1;
            self.1[ptr] = i;
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_u16(&mut self, i: u16) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 2;
        if end <= L {
            self.0 = end;
            unsafe { self.write_obj_internal(ptr, i.to_be()) };
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_u32(&mut self, i: u32) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 4;
        if end <= L {
            self.0 = end;
            unsafe { self.write_obj_internal(ptr, i.to_be()) };
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_u64(&mut self, i: u64) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 8;
        if end <= L {
            self.0 = end;
            unsafe { self.write_obj_internal(ptr, i.to_be()) };
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn append_u64_le(&mut self, i: u64) -> std::io::Result<()> {
        let ptr = self.0;
        let end = ptr + 8;
        if end <= L {
            self.0 = end;
            unsafe { self.write_obj_internal(ptr, i.to_le()) };
            Ok(())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn bytes_fixed_at<const S: usize>(&self, ptr: usize) -> std::io::Result<&[u8; S]> {
        if (ptr + S) <= self.0 {
            unsafe { Ok(&*self.1.as_ptr().cast::<u8>().add(ptr).cast::<[u8; S]>()) }
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn bytes_fixed_mut_at<const S: usize>(&mut self, ptr: usize) -> std::io::Result<&mut [u8; S]> {
        if (ptr + S) <= self.0 {
            unsafe { Ok(&mut *self.1.as_mut_ptr().cast::<u8>().add(ptr).cast::<[u8; S]>()) }
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn struct_at<T: Copy>(&self, ptr: usize) -> std::io::Result<&T> {
        if (ptr + size_of::<T>()) <= self.0 {
            unsafe { Ok(&*self.1.as_ptr().cast::<u8>().add(ptr).cast::<T>()) }
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn struct_mut_at<T: Copy>(&mut self, ptr: usize) -> std::io::Result<&mut T> {
        if (ptr + size_of::<T>()) <= self.0 {
            unsafe { Ok(&mut *self.1.as_mut_ptr().cast::<u8>().offset(ptr as isize).cast::<T>()) }
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn u8_at(&self, ptr: usize) -> std::io::Result<u8> {
        if ptr < self.0 {
            Ok(self.1[ptr])
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn u16_at(&self, ptr: usize) -> std::io::Result<u16> {
        let end = ptr + 2;
        debug_assert!(end <= L);
        if end <= self.0 {
            Ok(u16::from_be(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn u32_at(&self, ptr: usize) -> std::io::Result<u32> {
        let end = ptr + 4;
        debug_assert!(end <= L);
        if end <= self.0 {
            Ok(u32::from_be(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn u64_at(&self, ptr: usize) -> std::io::Result<u64> {
        let end = ptr + 8;
        debug_assert!(end <= L);
        if end <= self.0 {
            Ok(u64::from_be(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_struct<T: Copy>(&self, cursor: &mut usize) -> std::io::Result<&T> {
        let ptr = *cursor;
        let end = ptr + size_of::<T>();
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            unsafe { Ok(&*self.1.as_ptr().cast::<u8>().offset(ptr as isize).cast::<T>()) }
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_bytes_fixed<const S: usize>(&self, cursor: &mut usize) -> std::io::Result<&[u8; S]> {
        let ptr = *cursor;
        let end = ptr + S;
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            unsafe { Ok(&*self.1.as_ptr().cast::<u8>().offset(ptr as isize).cast::<[u8; S]>()) }
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_bytes(&self, l: usize, cursor: &mut usize) -> std::io::Result<&[u8]> {
        let ptr = *cursor;
        let end = ptr + l;
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            Ok(&self.1[ptr..end])
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_varint(&self, cursor: &mut usize) -> std::io::Result<u64> {
        let c = *cursor;
        if c < self.0 {
            let mut a = &self.1[c..];
            varint::read(&mut a).map(|r| {
                *cursor = c + r.1;
                debug_assert!(*cursor <= self.0);
                r.0
            })
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_u8(&self, cursor: &mut usize) -> std::io::Result<u8> {
        let ptr = *cursor;
        debug_assert!(ptr < L);
        if ptr < self.0 {
            *cursor = ptr + 1;
            Ok(self.1[ptr])
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_u16(&self, cursor: &mut usize) -> std::io::Result<u16> {
        let ptr = *cursor;
        let end = ptr + 2;
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            Ok(u16::from_be(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_u32(&self, cursor: &mut usize) -> std::io::Result<u32> {
        let ptr = *cursor;
        let end = ptr + 4;
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            Ok(u32::from_be(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_u64(&self, cursor: &mut usize) -> std::io::Result<u64> {
        let ptr = *cursor;
        let end = ptr + 8;
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            Ok(u64::from_be(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    pub fn read_u64_le(&self, cursor: &mut usize) -> std::io::Result<u64> {
        let ptr = *cursor;
        let end = ptr + 8;
        debug_assert!(end <= L);
        if end <= self.0 {
            *cursor = end;
            Ok(u64::from_le(unsafe { self.read_obj_internal(ptr) }))
        } else {
            Err(overflow_err())
        }
    }
}

impl<const L: usize> Write for Buffer<L> {
    #[inline(always)]
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let ptr = self.0;
        let end = ptr + buf.len();
        if end <= L {
            self.0 = end;
            self.1[ptr..end].copy_from_slice(buf);
            Ok(buf.len())
        } else {
            Err(overflow_err())
        }
    }

    #[inline(always)]
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl<const L: usize> AsRef<[u8]> for Buffer<L> {
    #[inline(always)]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl<const L: usize> AsMut<[u8]> for Buffer<L> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [u8] {
        self.as_bytes_mut()
    }
}

impl<const L: usize> From<[u8; L]> for Buffer<L> {
    #[inline(always)]
    fn from(a: [u8; L]) -> Self {
        Self(L, a)
    }
}

impl<const L: usize> From<&[u8; L]> for Buffer<L> {
    #[inline(always)]
    fn from(a: &[u8; L]) -> Self {
        Self(L, a.clone())
    }
}

/// Implements std::io::Read for a buffer and a cursor.
pub struct BufferReader<'a, 'b, const L: usize>(&'a Buffer<L>, &'b mut usize);

impl<'a, 'b, const L: usize> BufferReader<'a, 'b, L> {
    #[inline(always)]
    pub fn new(b: &'a Buffer<L>, cursor: &'b mut usize) -> Self {
        Self(b, cursor)
    }
}

impl<'a, 'b, const L: usize> Read for BufferReader<'a, 'b, L> {
    #[inline(always)]
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        buf.copy_from_slice(self.0.read_bytes(buf.len(), self.1)?);
        Ok(buf.len())
    }
}

pub struct PooledBufferFactory<const L: usize>;

impl<const L: usize> PooledBufferFactory<L> {
    #[inline(always)]
    pub fn new() -> Self {
        Self {}
    }
}

impl<const L: usize> PoolFactory<Buffer<L>> for PooledBufferFactory<L> {
    #[inline(always)]
    fn create(&self) -> Buffer<L> {
        Buffer::new()
    }

    #[inline(always)]
    fn reset(&self, obj: &mut Buffer<L>) {
        obj.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::Buffer;

    #[test]
    fn buffer_basic_u64() {
        let mut b = Buffer::<8>::new();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
        assert!(b.append_u64(1234).is_ok());
        assert_eq!(b.len(), 8);
        assert!(!b.is_empty());
        assert_eq!(b.read_u64(&mut 0).unwrap(), 1234);
        b.clear();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
    }

    #[test]
    fn buffer_basic_u32() {
        let mut b = Buffer::<4>::new();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
        assert!(b.append_u32(1234).is_ok());
        assert_eq!(b.len(), 4);
        assert!(!b.is_empty());
        assert_eq!(b.read_u32(&mut 0).unwrap(), 1234);
        b.clear();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
    }

    #[test]
    fn buffer_basic_u16() {
        let mut b = Buffer::<2>::new();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
        assert!(b.append_u16(1234).is_ok());
        assert_eq!(b.len(), 2);
        assert!(!b.is_empty());
        assert_eq!(b.read_u16(&mut 0).unwrap(), 1234);
        b.clear();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
    }

    #[test]
    fn buffer_basic_u8() {
        let mut b = Buffer::<1>::new();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
        assert!(b.append_u8(128).is_ok());
        assert_eq!(b.len(), 1);
        assert!(!b.is_empty());
        assert_eq!(b.read_u8(&mut 0).unwrap(), 128);
        b.clear();
        assert_eq!(b.len(), 0);
        assert!(b.is_empty());
    }

    #[test]
    fn buffer_bytes() {
        const SIZE: usize = 100;

        for _ in 0..1000 {
            let mut v: Vec<u8> = Vec::with_capacity(SIZE);
            v.fill_with(|| rand::random());

            let mut b = Buffer::<SIZE>::new();
            assert!(b.append_bytes(&v).is_ok());
            assert_eq!(b.read_bytes(v.len(), &mut 0).unwrap(), &v);

            let mut v: [u8; SIZE] = [0u8; SIZE];
            v.fill_with(|| rand::random());

            let mut b = Buffer::<SIZE>::new();
            assert!(b.append_bytes_fixed(&v).is_ok());
            assert_eq!(b.read_bytes_fixed(&mut 0).unwrap(), &v);

            // FIXME: append calls for _get_mut style do not accept anything to append, so we can't
            // test them.
            //
            // let mut b = Buffer::<SIZE>::new();
            // let res = b.append_bytes_fixed_get_mut(&v);
            // assert!(res.is_ok());
            // let byt = res.unwrap();
            // assert_eq!(byt, &v);
        }
    }

    #[test]
    fn buffer_at() {
        const SIZE: usize = 100;

        for _ in 0..1000 {
            let mut v = [0u8; SIZE];
            let mut idx: usize = rand::random::<usize>() % SIZE;
            v[idx] = 1;

            let mut b = Buffer::<SIZE>::new();
            assert!(b.append_bytes(&v).is_ok());

            let res = b.bytes_fixed_at::<1>(idx);
            assert!(res.is_ok());
            assert_eq!(res.unwrap()[0], 1);

            let res = b.bytes_fixed_mut_at::<1>(idx);
            assert!(res.is_ok());
            assert_eq!(res.unwrap()[0], 1);

            // the uX integer tests require a little more massage. we're going to rewind the index
            // by 8, correcting to 0 if necessary, and then write 1's in. our numbers will be
            // consistent this way.
            v[idx] = 0;

            if idx < 8 {
                idx = 0;
            } else if (idx + 7) >= SIZE {
                idx -= 7;
            }

            for i in idx..(idx + 8) {
                v[i] = 1;
            }

            let mut b = Buffer::<SIZE>::new();
            assert!(b.append_bytes(&v).is_ok());

            let res = b.u8_at(idx);
            assert!(res.is_ok());
            assert_eq!(res.unwrap(), 1);

            let res = b.u16_at(idx);
            assert!(res.is_ok());
            assert_eq!(res.unwrap(), 257);

            let res = b.u32_at(idx);
            assert!(res.is_ok());
            assert_eq!(res.unwrap(), 16843009);

            let res = b.u64_at(idx);
            assert!(res.is_ok());
            assert_eq!(res.unwrap(), 72340172838076673);
        }
    }

    #[test]
    fn buffer_sizing() {
        const SIZE: usize = 100;

        for _ in 0..1000 {
            let v = [0u8; SIZE];
            let mut b = Buffer::<SIZE>::new();
            assert!(b.append_bytes(&v).is_ok());
            assert_eq!(b.len(), SIZE);
            b.set_size(10);
            assert_eq!(b.len(), 10);
            unsafe {
                b.set_size_unchecked(8675309);
            }
            assert_eq!(b.len(), 8675309);
        }
    }
}
