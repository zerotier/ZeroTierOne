use std::convert::TryInto;
use std::ptr::write_volatile;

/// Container for secrets that clears them on drop.
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
}

impl<const L: usize> Drop for Secret<L> {
    fn drop(&mut self) {
        let p = self.0.as_mut_ptr();
        for i in 0..L {
            unsafe { write_volatile(p.offset(i as isize), 0_u8) };
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
