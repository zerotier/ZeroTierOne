use core::arch::x86_64::*;

pub fn verify(a: &[u8], b: &[u8], mut len: usize) -> u8 {
    let (mut f, mut g);
    let mut r: u64;
    unsafe {
        let mut h = _mm256_setzero_si256();
        for i in 0..(len / 32) {
            f = _mm256_loadu_si256(a[32 * i..].as_ptr() as *const __m256i);
            g = _mm256_loadu_si256(b[32 * i..].as_ptr() as *const __m256i);
            f = _mm256_xor_si256(f, g);
            h = _mm256_or_si256(h, f);
        }
        r = 1 - _mm256_testz_si256(h, h) as u64;
    }
    let idx = 32 * (len / 32);
    len -= idx;
    for i in 0..len {
        r |= (a[idx + i] ^ b[idx + i]) as u64;
    }
    (r.wrapping_neg() >> 63) as u8
}

pub fn cmov(r: &mut [u8], x: &[u8], mut len: usize, mut b: u8) {
    let (mut xvec, mut rvec);
    unsafe {
        let bvec = _mm256_set1_epi64x(b as i64);
        for i in 0..(len / 32) {
            rvec = _mm256_loadu_si256(r[32 * i..].as_ptr() as *const __m256i);
            xvec = _mm256_loadu_si256(x[32 * i..].as_ptr() as *const __m256i);
            rvec = _mm256_blendv_epi8(rvec, xvec, bvec);
            _mm256_storeu_si256(r[32 * i..].as_mut_ptr() as *mut __m256i, rvec);
        }
    }
    let idx = 32 * (len / 32);
    len -= idx;
    b = b.wrapping_neg();
    for i in 0..len {
        r[idx + i] ^= b & (x[idx + i] ^ r[idx + i]);
    }
}
