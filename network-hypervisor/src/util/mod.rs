pub mod hex;
pub mod pool;

pub(crate) const ZEROES: [u8; 64] = [0_u8; 64];

#[inline(always)]
pub(crate) unsafe fn equal_bytes(a: *const u8, b: *const u8, l: usize) -> bool {
    for i in 0..l {
        if *a.offset(i as isize) != *b.offset(i as isize) {
            return false;
        }
    }
    true
}

#[inline(always)]
pub(crate) fn integer_store_be_u16(i: u16, d: &mut [u8]) {
    d[0] = (i >> 8) as u8;
    d[1] = i as u8;
}

#[inline(always)]
pub(crate) fn integer_store_be_u32(i: u32, d: &mut [u8]) {
    d[0] = (i >> 24) as u8;
    d[1] = (i >> 16) as u8;
    d[2] = (i >> 8) as u8;
    d[3] = i as u8;
}

#[inline(always)]
pub(crate) fn integer_store_be_u64(i: u64, d: &mut [u8]) {
    d[0] = (i >> 56) as u8;
    d[1] = (i >> 48) as u8;
    d[2] = (i >> 40) as u8;
    d[3] = (i >> 32) as u8;
    d[4] = (i >> 24) as u8;
    d[5] = (i >> 16) as u8;
    d[6] = (i >> 8) as u8;
    d[7] = i as u8;
}

#[inline(always)]
pub(crate) fn integer_load_be_u16(d: &[u8]) -> u16 {
    (d[0] as u16) << 8 | (d[1] as u16)
}

#[inline(always)]
pub(crate) fn integer_load_be_u32(d: &[u8]) -> u32 {
    (d[0] as u32) << 24 | (d[1] as u32) << 16 | (d[2] as u32) << 8 | (d[3] as u32)
}

#[inline(always)]
pub(crate) fn integer_load_be_u64(d: &[u8]) -> u64 {
    (d[0] as u64) << 56 | (d[1] as u64) << 48 | (d[2] as u64) << 40 | (d[3] as u64) << 32 | (d[4] as u64) << 24 | (d[5] as u64) << 16 | (d[6] as u64) << 8 | (d[7] as u64)
}

/// Mix bits in a 64-bit integer.
/// https://nullprogram.com/blog/2018/07/31/
pub(crate) fn hash64(mut x: u64) -> u64 {
    x ^= x.wrapping_shr(30);
    x = x.wrapping_mul(0xbf58476d1ce4e5b9);
    x ^= x.wrapping_shr(27);
    x = x.wrapping_mul(0x94d049bb133111eb);
    x ^ x.wrapping_shr(31)
}

/// Mix bits in 32-bit integer.
/// https://nullprogram.com/blog/2018/07/31/
#[inline(always)]
pub(crate) fn hash32(mut x: u32) -> u32 {
    x ^= x.wrapping_shr(16);
    x = x.wrapping_mul(0x7feb352d);
    x ^= x.wrapping_shr(15);
    x = x.wrapping_mul(0x846ca68b);
    x ^ x.wrapping_shr(16)
}
