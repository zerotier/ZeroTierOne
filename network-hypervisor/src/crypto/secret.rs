use std::convert::TryInto;
use std::mem::size_of;
use std::ptr::write_volatile;

/// Container for secrets that clears them on drop.
///
/// We can't be totally sure that things like libraries are doing this and it's
/// hard to get every use of a secret anywhere, but using this in our code at
/// least reduces the number of secrets that are left lying around in memory.
///
/// This is generally a low-risk thing since it's process memory that's protected,
/// but it's still not a bad idea due to things like swap or obscure side channel
/// attacks that allow memory to be read.
#[derive(Clone, PartialEq, Eq)]
pub struct Secret<const L: usize>(pub(crate) [u8; L]);

impl<const L: usize> Secret<L> {
    #[inline(always)]
    pub fn new() -> Self {
        Self([0_u8; L])
    }

    /// Copy bytes into secret, will panic if size does not match.
    #[inline(always)]
    pub fn from_bytes(b: &[u8]) -> Self {
        Self(b.try_into().unwrap())
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; L] {
        return &self.0
    }
}

impl<const L: usize> Drop for Secret<L> {
    fn drop(&mut self) {
        unsafe {
            let p = self.0.as_mut_ptr();
            if (L % size_of::<usize>()) == 0 {
                for i in 0..(L / size_of::<usize>()) {
                    write_volatile(p.cast::<usize>().offset(i as isize), 0_usize);
                }
            } else {
                for i in 0..L {
                    write_volatile(p.offset(i as isize), 0_u8);
                }
            }
        }
    }
}

impl<const L: usize> Default for Secret<L> {
    #[inline(always)]
    fn default() -> Self {
        Self([0_u8; L])
    }
}

impl<const L: usize> AsRef<[u8]> for Secret<L> {
    #[inline(always)]
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl<const L: usize> AsRef<[u8; L]> for Secret<L> {
    #[inline(always)]
    fn as_ref(&self) -> &[u8; L] {
        &self.0
    }
}

impl<const L: usize> AsMut<[u8]> for Secret<L> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [u8] {
        &mut self.0
    }
}

impl<const L: usize> AsMut<[u8; L]> for Secret<L> {
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [u8; L] {
        &mut self.0
    }
}
