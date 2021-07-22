use std::cmp::Ordering;
use std::ops::{Add, AddAssign, Sub, Shl, Shr, SubAssign, ShlAssign, ShrAssign};
use std::io::Write;
use std::mem::MaybeUninit;

const HEX_CHARS: [char; 16] = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'];

/// Arbitrarily large unsigned integer supporting basic and modular arithmetic.
///
/// LIMBS is the number of 64-bit "limbs" (large digits) in this VLI. The number of bits is
/// 64 times LIMBS, so for 1024 bits use 16 LIMBS. We don't have a generic BITS instead
/// because of current limitations on Rust const generics and what can be done with them.
/// This also means the size of a VLI must be a multiple of 64 bits. Note that the actual
/// integer in it need not be exactly that length, just the capacity of the container.
#[derive(Clone, PartialEq, Eq)]
#[repr(transparent)]
pub struct VLI<const LIMBS: usize> {
    n: [u64; LIMBS]
}

impl<const LIMBS: usize> Default for VLI<LIMBS> {
    #[inline(always)]
    fn default() -> Self {
        Self { n: [0_u64; LIMBS ]}
    }
}

impl<const LIMBS: usize> VLI<LIMBS> {
    /// Create a new zero VLI.
    #[inline(always)]
    pub fn new() -> Self {
        Self { n: [0_u64; LIMBS ]}
    }

    /// Get an integer with an undefined value.
    /// This can be used in high performance code to avoid zeroing on new() when the
    /// result will be overwritten right away by an operation.
    #[inline(always)]
    pub unsafe fn uninit() -> Self {
        MaybeUninit::<VLI<LIMBS>>::uninit().assume_init()
    }

    /// Set to zero.
    #[inline(always)]
    pub fn zero(&mut self) {
        self.n.fill(0)
    }

    /// Test whether bit is set (numbered from right to left).
    /// This will panic if bit is out of range.
    #[inline(always)]
    pub fn test_bit(&self, bit: usize) -> bool {
        (self.n[bit >> 6] & (1_u64 << (bit & 63))) != 0
    }

    /// Count the number of non-zero bits.
    pub fn count_ones(&self) -> u32 {
        let mut ones = 0_u32;
        for n in self.n {
            ones += n.count_ones();
        }
        ones
    }

    /// Count the number of zero bits.
    #[inline(always)]
    pub fn count_zeros(&self) -> u32 {
        let mut zeros = 0_u32;
        for n in self.n {
            zeros += n.count_zeros();
        }
        zeros
    }

    /// Returns true if this integer is zero.
    #[inline(always)]
    pub fn is_zero(&self) -> bool {
        for n in self.n {
            if n != 0 {
                return false;
            }
        }
        true
    }

    /// Returns true if this is an odd number.
    #[inline(always)]
    pub fn is_odd(&self) -> bool {
        (self.n[0] & 1) != 0
    }

    /// Returns true if this is an even number.
    #[inline(always)]
    pub fn is_even(&self) -> bool {
        (self.n[0] & 1) == 0
    }

    /// Add to this integer and return any overflow carry bits.
    pub fn add_assign_carry(&mut self, rhs: &Self) -> u64 {
        let mut carry = 0_u64;
        for i in 0..LIMBS {
            let left_ptr = unsafe { self.n.get_unchecked_mut(i) };
            let left = *left_ptr;
            let sum = left + *unsafe { rhs.n.get_unchecked(i) } + carry;
            carry = (sum < left) as u64;
            *left_ptr = sum;
        }
        carry
    }

    /// Subtract from this integer and return any remaining borrow bits.
    pub fn sub_assign_borrow(&mut self, rhs: &Self) -> u64 {
        let mut borrow = 0_u64;
        for i in 0..LIMBS {
            let left_ptr = unsafe { self.n.get_unchecked_mut(i) };
            let left = *left_ptr;
            let diff = left - *unsafe { rhs.n.get_unchecked(i) } - borrow;
            borrow = (diff > left) as u64;
            *left_ptr = diff;
        }
        borrow
    }

    /// Multiply two inputs half the size of this integer to yield a full size result in this integer.
    /// The multiplicand sizes MULT_LIMBS must be one half the LIMBS size of this integer.
    /// This is checked with an assertion. This isn't computed with the type system due
    /// to current limitations in const generics.
    pub fn mul_assign_widening<const MULT_LIMBS: usize>(&mut self, lhs: &VLI<{ MULT_LIMBS }>, rhs: &VLI<{ MULT_LIMBS }>) {
        assert_eq!(MULT_LIMBS, LIMBS / 2);
        let mut r01 = 0_u128;
        let mut r2 = 0_u64;
        let mut k = 0_usize;
        while k < MULT_LIMBS {
            for i in 0..k {
                debug_assert!(i < MULT_LIMBS);
                debug_assert!((k - i) < MULT_LIMBS);
                let l_product = (*unsafe { lhs.n.get_unchecked(i) } as u128) * (*unsafe { rhs.n.get_unchecked(k - i) } as u128);
                r01 += l_product;
                r2 += (r01 < l_product) as u64;
            }
            *unsafe { self.n.get_unchecked_mut(k) } = r01 as u64;
            r01 += (r01 >> 64) | ((r2 as u128) << 64);
            r2 = 0;
            k += 1;
        }
        while k < (LIMBS - 1) {
            for i in ((k + 1) - MULT_LIMBS)..k {
                debug_assert!(i < MULT_LIMBS);
                debug_assert!((k - i) < MULT_LIMBS);
                let l_product = (*unsafe { lhs.n.get_unchecked(i) } as u128) * (*unsafe { rhs.n.get_unchecked(k - i) } as u128);
                r01 += l_product;
                r2 += (r01 < l_product) as u64;
            }
            *unsafe { self.n.get_unchecked_mut(k) } = r01 as u64;
            r01 += (r01 >> 64) | ((r2 as u128) << 64);
            r2 = 0;
            k += 1;
        }
        *unsafe { self.n.get_unchecked_mut(LIMBS - 1) } = r01 as u64;
    }

    /// Get this integer as a big-endian byte array.
    /// If skip_leading_zeroes is true the returned byte vector will be the minimum size
    /// needed to hold the integer, or empty if it is zero. Otherwise it will always be
    /// LIMBS * 8 bytes in length.
    pub fn to_be_bytes(&self, skip_leading_zeroes: bool) -> Vec<u8> {
        let mut bytes: Vec<u8> = Vec::new();
        bytes.reserve(LIMBS * 8);

        let mut i = LIMBS as isize - 1;
        if skip_leading_zeroes {
            while i >= 0 {
                let x: u64 = self.n[i as usize];
                if x != 0 {
                    let x = x.to_be_bytes();
                    for j in 0..8 {
                        if x[j] != 0 {
                            let _ = bytes.write_all(&x[j..8]);
                            break;
                        }
                    }
                    break;
                }
                i -= 1;
            }
        }
        while i >= 0 {
            let _ = bytes.write_all(&self.n[i as usize].to_be_bytes());
            i -= 1;
        }

        bytes
    }

    /// Get this integer as a hex string.
    /// If skip_leading_zeroes is true, the returned string will not be left padded with zeroes
    /// to the size it would be if the integer's bits were saturated.
    pub fn to_hex_string(&self, skip_leading_zeroes: bool) -> String {
        let mut s = String::new();
        s.reserve(LIMBS * 16);

        let mut i = LIMBS as isize - 1;
        if skip_leading_zeroes {
            while i >= 0 {
                let mut x = self.n[i as usize];
                if x != 0 {
                    let mut j = 0;
                    while j < 16 {
                        if (x >> 60) != 0 {
                            break;
                        }
                        x <<= 4;
                        j += 1;
                    }
                    while j < 16 {
                        s.push(HEX_CHARS[(x >> 60) as usize]);
                        x <<= 4;
                        j += 1;
                    }
                    break;
                }
                i -= 1;
            }
        }
        while i >= 0 {
            let mut x: u64 = self.n[i as usize];
            for _ in 0..16 {
                s.push(HEX_CHARS[(x >> 60) as usize]);
                x <<= 4;
            }
            i -= 1;
        }

        if s.is_empty() {
            s.push('0');
        }

        s
    }
}

impl<const LIMBS: usize> Add<&Self> for VLI<LIMBS> {
    type Output = Self;

    #[inline(always)]
    fn add(mut self, rhs: &Self) -> Self::Output {
        let _ = self.add_assign_carry(rhs);
        self
    }
}

impl<const LIMBS: usize> AddAssign<&Self> for VLI<LIMBS> {
    #[inline(always)]
    fn add_assign(&mut self, rhs: &Self) {
        let _ = self.add_assign_carry(rhs);
    }
}

impl<const LIMBS: usize> Sub<&Self> for VLI<LIMBS> {
    type Output = Self;

    #[inline(always)]
    fn sub(mut self, rhs: &Self) -> Self::Output {
        let _ = self.sub_assign_borrow(rhs);
        self
    }
}

impl<const LIMBS: usize> SubAssign<&Self> for VLI<LIMBS> {
    #[inline(always)]
    fn sub_assign(&mut self, rhs: &Self) {
        let _ = self.sub_assign_borrow(rhs);
    }
}

impl<const LIMBS: usize> Shl<usize> for VLI<LIMBS> {
    type Output = Self;

    #[inline(always)]
    fn shl(mut self, rhs: usize) -> Self::Output {
        self.shl_assign(rhs);
        self
    }
}

impl<const LIMBS: usize> ShlAssign<usize> for VLI<LIMBS> {
    fn shl_assign(&mut self, rhs: usize) {
        if rhs != 0 {
            if rhs < (LIMBS * 64) {
                let whole_limb_shifts = rhs >> 6;
                if whole_limb_shifts != 0 {
                    self.n.copy_within(0..(LIMBS - whole_limb_shifts), whole_limb_shifts);
                    self.n[0..whole_limb_shifts].fill(0);
                }
                let rhs = rhs & 63;
                let mut carry = 0_u64;
                for i in 0..LIMBS {
                    let x_ptr = unsafe { self.n.get_unchecked_mut(i) };
                    let x = *x_ptr;
                    *x_ptr = (x << rhs) | carry;
                    carry = x >> (64 - rhs);
                }
            } else {
                self.zero();
            }
        }
    }
}

impl<const LIMBS: usize> Shr<usize> for VLI<LIMBS> {
    type Output = Self;

    #[inline(always)]
    fn shr(mut self, rhs: usize) -> Self::Output {
        self.shr_assign(rhs);
        self
    }
}

impl<const LIMBS: usize> ShrAssign<usize> for VLI<LIMBS> {
    fn shr_assign(&mut self, rhs: usize) {
        if rhs != 0 {
            if rhs < (LIMBS * 64) {
                let whole_limb_shifts = rhs >> 6;
                if whole_limb_shifts != 0 {
                    self.n.copy_within(whole_limb_shifts..LIMBS, 0);
                    self.n[(LIMBS - whole_limb_shifts)..LIMBS].fill(0);
                }
                let rhs = rhs & 63;
                let mut carry = 0_u64;
                let mut i = LIMBS as isize - 1;
                while i >= 0 {
                    let x_ptr = unsafe { self.n.get_unchecked_mut(i as usize) };
                    let x = *x_ptr;
                    *x_ptr = (x >> rhs) | carry;
                    carry = x << (64 - rhs);
                    i -= 1;
                }
            } else {
                self.zero();
            }
        }
    }
}

impl<const LIMBS: usize> PartialOrd for VLI<LIMBS> {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }

    fn lt(&self, other: &Self) -> bool {
        let mut i = LIMBS as isize - 1;
        while i >= 0 {
            let a = *unsafe { self.n.get_unchecked(i as usize) };
            let b = *unsafe { other.n.get_unchecked(i as usize) };
            if a > b {
                return false;
            } else if a < b {
                return true;
            }
            i -= 1;
        }
        false
    }

    fn le(&self, other: &Self) -> bool {
        let mut i = LIMBS as isize - 1;
        while i >= 0 {
            let a = *unsafe { self.n.get_unchecked(i as usize) };
            let b = *unsafe { other.n.get_unchecked(i as usize) };
            if a > b {
                return false;
            } else if a < b {
                return true;
            }
            i -= 1;
        }
        true
    }

    fn gt(&self, other: &Self) -> bool {
        let mut i = LIMBS as isize - 1;
        while i >= 0 {
            let a = *unsafe { self.n.get_unchecked(i as usize) };
            let b = *unsafe { other.n.get_unchecked(i as usize) };
            if a > b {
                return true;
            } else if a < b {
                return false;
            }
            i -= 1;
        }
        false
    }

    fn ge(&self, other: &Self) -> bool {
        let mut i = LIMBS as isize - 1;
        while i >= 0 {
            let a = *unsafe { self.n.get_unchecked(i as usize) };
            let b = *unsafe { other.n.get_unchecked(i as usize) };
            if a > b {
                return true;
            } else if a < b {
                return false;
            }
            i -= 1;
        }
        true
    }
}

impl<const LIMBS: usize> Ord for VLI<LIMBS> {
    fn cmp(&self, other: &Self) -> Ordering {
        let mut i = LIMBS as isize - 1;
        while i >= 0 {
            let a = *unsafe { self.n.get_unchecked(i as usize) };
            let b = *unsafe { other.n.get_unchecked(i as usize) };
            if a > b {
                return Ordering::Greater;
            } else if a < b {
                return Ordering::Less;
            }
            i -= 1;
        }
        Ordering::Equal
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn arithmetic() {
    }
}
