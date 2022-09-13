#![allow(unused_imports)]
use crate::{align::*, cbd::*, consts::*, fips202::*, fips202x4::*, params::*, symmetric::*};
use core::arch::x86_64::*;

pub(crate) const NOISE_NBLOCKS: usize = (KYBER_ETA1 * KYBER_N / 4 + SHAKE256_RATE - 1) / SHAKE256_RATE;

#[derive(Clone)]
#[repr(C)]
pub union Poly {
    pub coeffs: [i16; KYBER_N],
    pub vec: [__m256i; (KYBER_N + 15) / 16],
}

impl Copy for Poly {}

impl Poly {
    pub fn new() -> Self {
        Poly { coeffs: [0i16; KYBER_N] }
    }
    // Basic polynomial value checking for development
    // #[cfg(debug_assertions)]
    // fn checksum(&self) -> i16 {
    //   unsafe{
    //     let mut out = 0;
    //     for x in &self.coeffs {
    //       out ^= x;
    //     }
    //     out
    //   }
    // }
}

extern "C" {
    fn ntt_avx(r: &mut [i16; KYBER_N], q_data: &[i16; 640]);
    fn invntt_avx(r: &mut [i16; KYBER_N], q_data: &[i16; 640]);
    fn nttunpack_avx(r: &mut [i16; KYBER_N], q_data: &[i16; 640]);
    fn basemul_avx(r: &mut [i16; KYBER_N], a: &[i16; KYBER_N], b: &[i16; KYBER_N], q_data: &[i16; 640]);
    fn tomont_avx(r: &mut [i16; KYBER_N], q_data: &[i16; 640]);
    fn reduce_avx(r: &mut [i16; KYBER_N], q_data: &[i16; 640]);
    fn ntttobytes_avx(r: *mut u8, a: &[i16; KYBER_N], q_data: &[i16; 640]);
    fn nttfrombytes_avx(r: *mut i16, a: *const u8, q_data: &[i16; 640]);
}

// #[target_feature(enable = "avx2")]
#[cfg(any(feature = "kyber512", not(feature = "kyber1024")))]
pub unsafe fn poly_compress(r: &mut [u8], a: Poly) {
    let (mut f0, mut f1, mut f2, mut f3);
    let v: __m256i = _mm256_load_si256(QDATA.vec[_16XV / 16..].as_ptr());
    let shift1: __m256i = _mm256_set1_epi16(1 << 9);
    let mask: __m256i = _mm256_set1_epi16(15);
    let shift2: __m256i = _mm256_set1_epi16((16 << 8) + 1);
    let permdidx: __m256i = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

    for i in 0..KYBER_N / 64 {
        f0 = _mm256_load_si256(&a.vec[4 * i + 0]);
        f1 = _mm256_load_si256(&a.vec[4 * i + 1]);
        f2 = _mm256_load_si256(&a.vec[4 * i + 2]);
        f3 = _mm256_load_si256(&a.vec[4 * i + 3]);
        f0 = _mm256_mulhi_epi16(f0, v);
        f1 = _mm256_mulhi_epi16(f1, v);
        f2 = _mm256_mulhi_epi16(f2, v);
        f3 = _mm256_mulhi_epi16(f3, v);
        f0 = _mm256_mulhrs_epi16(f0, shift1);
        f1 = _mm256_mulhrs_epi16(f1, shift1);
        f2 = _mm256_mulhrs_epi16(f2, shift1);
        f3 = _mm256_mulhrs_epi16(f3, shift1);
        f0 = _mm256_and_si256(f0, mask);
        f1 = _mm256_and_si256(f1, mask);
        f2 = _mm256_and_si256(f2, mask);
        f3 = _mm256_and_si256(f3, mask);
        f0 = _mm256_packus_epi16(f0, f1);
        f2 = _mm256_packus_epi16(f2, f3);
        f0 = _mm256_maddubs_epi16(f0, shift2);
        f2 = _mm256_maddubs_epi16(f2, shift2);
        f0 = _mm256_packus_epi16(f0, f2);
        f0 = _mm256_permutevar8x32_epi32(f0, permdidx);
        _mm256_storeu_si256(r[32 * i..].as_mut_ptr() as *mut __m256i, f0);
    }
}
// #[target_feature(enable = "avx2")]
#[cfg(any(feature = "kyber512", not(feature = "kyber1024")))]
pub unsafe fn poly_decompress(r: &mut Poly, a: &[u8]) {
    let (mut t, mut f);
    let q: __m256i = _mm256_load_si256(QDATA.vec[_16XQ / 16..].as_ptr());
    let shufbidx: __m256i = _mm256_set_epi8(
        7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0,
    );
    let mask: __m256i = _mm256_set1_epi32(0x00F0000F);
    let shift: __m256i = _mm256_set1_epi32((128 << 16) + 2048);

    for i in 0..KYBER_N / 16 {
        t = _mm_loadl_epi64(a[8 * i..].as_ptr() as *const __m128i);
        f = _mm256_broadcastsi128_si256(t);
        f = _mm256_shuffle_epi8(f, shufbidx);
        f = _mm256_and_si256(f, mask);
        f = _mm256_mullo_epi16(f, shift);
        f = _mm256_mulhrs_epi16(f, q);
        _mm256_store_si256(&mut r.vec[i], f);
    }
}

// #[target_feature(enable = "avx2")]
#[cfg(feature = "kyber1024")]
pub unsafe fn poly_compress(r: &mut [u8], a: Poly) {
    let (mut f0, mut f1);
    let (mut t0, mut t1);
    let mut tmp;
    let v: __m256i = _mm256_load_si256(&QDATA.vec[_16XV / 16]);
    let shift1: __m256i = _mm256_set1_epi16(1 << 10);
    let mask: __m256i = _mm256_set1_epi16(31);
    let shift2: __m256i = _mm256_set1_epi16((32 << 8) + 1);
    let shift3: __m256i = _mm256_set1_epi32((1024 << 16) + 1);
    let sllvdidx: __m256i = _mm256_set1_epi64x(12);
    let shufbidx: __m256i = _mm256_set_epi8(
        8, -1, -1, -1, -1, -1, 4, 3, 2, 1, 0, -1, 12, 11, 10, 9, -1, 12, 11, 10, 9, 8, -1, -1, -1, -1, -1, 4, 3, 2, 1, 0,
    );

    for i in 0..(KYBER_N / 32) {
        f0 = _mm256_load_si256(&a.vec[2 * i + 0]);
        f1 = _mm256_load_si256(&a.vec[2 * i + 1]);
        f0 = _mm256_mulhi_epi16(f0, v);
        f1 = _mm256_mulhi_epi16(f1, v);
        f0 = _mm256_mulhrs_epi16(f0, shift1);
        f1 = _mm256_mulhrs_epi16(f1, shift1);
        f0 = _mm256_and_si256(f0, mask);
        f1 = _mm256_and_si256(f1, mask);
        f0 = _mm256_packus_epi16(f0, f1);
        f0 = _mm256_maddubs_epi16(f0, shift2);
        f0 = _mm256_madd_epi16(f0, shift3);
        f0 = _mm256_sllv_epi32(f0, sllvdidx);
        f0 = _mm256_srlv_epi64(f0, sllvdidx);
        f0 = _mm256_shuffle_epi8(f0, shufbidx);
        t0 = _mm256_castsi256_si128(f0);
        t1 = _mm256_extracti128_si256(f0, 1);
        t0 = _mm_blendv_epi8(t0, t1, _mm256_castsi256_si128(shufbidx));
        _mm_storeu_si128(r[20 * i + 0..].as_mut_ptr() as *mut __m128i, t0);
        tmp = _mm_cvtsi128_si32(t1);
        r[20 * i + 16..20 * i + 20].copy_from_slice(&tmp.to_le_bytes());
    }
}

// #[target_feature(enable = "avx2")]
#[cfg(feature = "kyber1024")]
pub unsafe fn poly_decompress(r: &mut Poly, a: &[u8]) {
    let (mut t, mut f, mut ti);

    let q = _mm256_load_si256(&QDATA.vec[_16XQ / 16]);
    let shufbidx = _mm256_set_epi8(
        9, 9, 9, 8, 8, 8, 8, 7, 7, 6, 6, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0,
    );
    let mask = _mm256_set_epi16(248, 1984, 62, 496, 3968, 124, 992, 31, 248, 1984, 62, 496, 3968, 124, 992, 31);
    let shift = _mm256_set_epi16(128, 16, 512, 64, 8, 256, 32, 1024, 128, 16, 512, 64, 8, 256, 32, 1024);

    for i in 0..KYBER_N / 16 {
        t = _mm_loadl_epi64(a[10 * i + 0..].as_ptr() as *const __m128i);
        ti = i32::from_le_bytes([a[10 * i + 8], a[10 * i + 9], 0, 0]);
        t = _mm_insert_epi16(t, ti, 4);
        f = _mm256_broadcastsi128_si256(t);
        f = _mm256_shuffle_epi8(f, shufbidx);
        f = _mm256_and_si256(f, mask);
        f = _mm256_mullo_epi16(f, shift);
        f = _mm256_mulhrs_epi16(f, q);
        _mm256_store_si256(r.vec[i..].as_mut_ptr() as *mut __m256i, f);
    }
}

pub fn poly_frombytes(r: &mut Poly, a: &[u8]) {
    unsafe {
        nttfrombytes_avx(r.coeffs.as_mut_ptr(), a.as_ptr(), &QDATA.coeffs);
    }
}

pub fn poly_tobytes(r: &mut [u8], a: Poly) {
    let mut buf = [0u8; KYBER_POLYBYTES];
    unsafe {
        ntttobytes_avx(buf.as_mut_ptr(), &a.coeffs, &QDATA.coeffs);
    }
    r[..KYBER_POLYBYTES].copy_from_slice(&buf[..]);
}

// #[target_feature(enable = "avx2")]
pub unsafe fn poly_frommsg(r: &mut Poly, msg: &[u8]) {
    let shift = _mm256_broadcastsi128_si256(_mm_set_epi32(0, 1, 2, 3));
    let idx = _mm256_broadcastsi128_si256(_mm_set_epi8(15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0));
    let hqs: __m256i = _mm256_set1_epi16((KYBER_Q + 1) as i16 / 2);
    let f = _mm256_loadu_si256(msg.as_ptr() as *const __m256i);

    let mut frommsg64 = |i: usize, mut g3: __m256i| {
        g3 = _mm256_sllv_epi32(g3, shift);
        g3 = _mm256_shuffle_epi8(g3, idx);
        let mut g0 = _mm256_slli_epi16(g3, 12);
        let mut g1 = _mm256_slli_epi16(g3, 8);
        let mut g2 = _mm256_slli_epi16(g3, 4);
        g0 = _mm256_srai_epi16(g0, 15);
        g1 = _mm256_srai_epi16(g1, 15);
        g2 = _mm256_srai_epi16(g2, 15);
        g3 = _mm256_srai_epi16(g3, 15);
        g0 = _mm256_and_si256(g0, hqs); // 19 18 17 16  3  2  1  0
        g1 = _mm256_and_si256(g1, hqs); // 23 22 21 20  7  6  5  4
        g2 = _mm256_and_si256(g2, hqs); // 27 26 25 24 11 10  9  8
        g3 = _mm256_and_si256(g3, hqs); // 31 30 29 28 15 14 13 12
        let h0 = _mm256_unpacklo_epi64(g0, g1);
        let h2 = _mm256_unpackhi_epi64(g0, g1);
        let h1 = _mm256_unpacklo_epi64(g2, g3);
        let h3 = _mm256_unpackhi_epi64(g2, g3);
        g0 = _mm256_permute2x128_si256(h0, h1, 0x20);
        g2 = _mm256_permute2x128_si256(h0, h1, 0x31);
        g1 = _mm256_permute2x128_si256(h2, h3, 0x20);
        g3 = _mm256_permute2x128_si256(h2, h3, 0x31);

        _mm256_store_si256(&mut r.vec[0 + 2 * i + 0], g0);
        _mm256_store_si256(&mut r.vec[0 + 2 * i + 1], g1);
        _mm256_store_si256(&mut r.vec[8 + 2 * i + 0], g2);
        _mm256_store_si256(&mut r.vec[8 + 2 * i + 1], g3);
    };

    frommsg64(0, _mm256_shuffle_epi32(f, 0));
    frommsg64(1, _mm256_shuffle_epi32(f, 85));
    frommsg64(2, _mm256_shuffle_epi32(f, 170));
    frommsg64(3, _mm256_shuffle_epi32(f, 255));
}

// #[target_feature(enable = "avx2")]
pub fn poly_tomsg(msg: &mut [u8], a: Poly) {
    unsafe {
        let (mut f0, mut f1, mut g0, mut g1);
        let hq: __m256i = _mm256_set1_epi16((KYBER_Q - 1) as i16 / 2);
        let hhq: __m256i = _mm256_set1_epi16((KYBER_Q - 1) as i16 / 4);

        for i in 0..KYBER_N / 32 {
            f0 = _mm256_load_si256(&a.vec[2 * i + 0]);
            f1 = _mm256_load_si256(&a.vec[2 * i + 1]);
            f0 = _mm256_sub_epi16(hq, f0);
            f1 = _mm256_sub_epi16(hq, f1);
            g0 = _mm256_srai_epi16(f0, 15);
            g1 = _mm256_srai_epi16(f1, 15);
            f0 = _mm256_xor_si256(f0, g0);
            f1 = _mm256_xor_si256(f1, g1);
            f0 = _mm256_sub_epi16(f0, hhq);
            f1 = _mm256_sub_epi16(f1, hhq);
            f0 = _mm256_packs_epi16(f0, f1);
            f0 = _mm256_permute4x64_epi64(f0, 0xD8);
            let small = _mm256_movemask_epi8(f0);
            msg[4 * i..][..4].copy_from_slice(&small.to_ne_bytes());
        }
    }
}

#[cfg(all(any(feature = "kyber1024", feature = "kyber512"), not(feature = "90s")))]
pub fn poly_getnoise_eta2(r: &mut Poly, seed: &[u8], nonce: u8) {
    let mut buf = Eta2Buf::new();
    unsafe {
        prf(&mut buf.coeffs, KYBER_ETA2 * KYBER_N / 4, seed, nonce);
        poly_cbd_eta2(r, &buf.vec);
    }
}

#[cfg(not(feature = "90s"))]
pub fn poly_getnoise_eta1_4x(
    r0: &mut Poly,
    r1: &mut Poly,
    r2: &mut Poly,
    r3: &mut Poly,
    seed: &[u8],
    nonce0: u8,
    nonce1: u8,
    nonce2: u8,
    nonce3: u8,
) {
    unsafe {
        let mut buf = [Eta4xBuf::new(); 4];
        let mut state = Keccakx4State::new();
        let f = _mm256_loadu_si256(seed.as_ptr() as *const __m256i);
        _mm256_store_si256(buf[0].vec.as_mut_ptr(), f);
        _mm256_store_si256(buf[1].vec.as_mut_ptr(), f);
        _mm256_store_si256(buf[2].vec.as_mut_ptr(), f);
        _mm256_store_si256(buf[3].vec.as_mut_ptr(), f);

        buf[0].coeffs[32] = nonce0;
        buf[1].coeffs[32] = nonce1;
        buf[2].coeffs[32] = nonce2;
        buf[3].coeffs[32] = nonce3;

        shake256x4_absorb_once(&mut state, &buf[0].coeffs, &buf[1].coeffs, &buf[2].coeffs, &buf[3].coeffs, 33);
        shake256x4_squeezeblocks(&mut buf, NOISE_NBLOCKS, &mut state);

        poly_cbd_eta1(r0, &buf[0]);
        poly_cbd_eta1(r1, &buf[1]);
        poly_cbd_eta1(r2, &buf[2]);
        poly_cbd_eta1(r3, &buf[3]);
    }
}

#[cfg(all(feature = "kyber512", not(feature = "90s")))]
pub fn poly_getnoise_eta1122_4x(
    r0: &mut Poly,
    r1: &mut Poly,
    r2: &mut Poly,
    r3: &mut Poly,
    seed: &[u8],
    nonce0: u8,
    nonce1: u8,
    nonce2: u8,
    nonce3: u8,
) {
    let mut buf = [Eta4xBuf::new(); 4];
    let mut state = Keccakx4State::new();
    unsafe {
        let f = _mm256_loadu_si256(seed.as_ptr() as *const __m256i);
        _mm256_store_si256(buf[0].vec.as_mut_ptr(), f);
        _mm256_store_si256(buf[1].vec.as_mut_ptr(), f);
        _mm256_store_si256(buf[2].vec.as_mut_ptr(), f);
        _mm256_store_si256(buf[3].vec.as_mut_ptr(), f);

        buf[0].coeffs[32] = nonce0;
        buf[1].coeffs[32] = nonce1;
        buf[2].coeffs[32] = nonce2;
        buf[3].coeffs[32] = nonce3;

        shake256x4_absorb_once(&mut state, &buf[0].coeffs, &buf[1].coeffs, &buf[2].coeffs, &buf[3].coeffs, 33);
        shake256x4_squeezeblocks(&mut buf, NOISE_NBLOCKS, &mut state);

        poly_cbd_eta1(r0, &buf[0]);
        poly_cbd_eta1(r1, &buf[1]);
        poly_cbd_eta2(r2, &buf[2].vec);
        poly_cbd_eta2(r3, &buf[3].vec);
    }
}

pub fn poly_ntt(r: &mut Poly) {
    unsafe {
        ntt_avx(&mut r.coeffs, &QDATA.coeffs);
    }
}

pub fn poly_invntt_tomont(r: &mut Poly) {
    unsafe {
        invntt_avx(&mut r.coeffs, &QDATA.coeffs);
    }
}

pub fn poly_nttunpack(r: &mut Poly) {
    unsafe {
        nttunpack_avx(&mut r.coeffs, &QDATA.coeffs);
    }
}

pub fn poly_basemul(r: &mut Poly, a: &Poly, b: &Poly) {
    unsafe {
        basemul_avx(&mut r.coeffs, &a.coeffs, &b.coeffs, &QDATA.coeffs);
    }
}

pub fn poly_tomont(r: &mut Poly) {
    unsafe {
        tomont_avx(&mut r.coeffs, &QDATA.coeffs);
    }
}

pub fn poly_reduce(r: &mut Poly) {
    unsafe {
        reduce_avx(&mut r.coeffs, &QDATA.coeffs);
    }
}

pub fn poly_add(r: &mut Poly, b: &Poly) {
    let (mut f0, mut f1);
    for i in 0..(KYBER_N / 16) {
        unsafe {
            f0 = _mm256_load_si256(&r.vec[i]);
            f1 = _mm256_load_si256(&b.vec[i]);
            f0 = _mm256_add_epi16(f0, f1);
            _mm256_store_si256(&mut r.vec[i], f0);
        }
    }
}

pub fn poly_sub(r: &mut Poly, a: &Poly) {
    let (mut f0, mut f1);
    for i in 0..(KYBER_N / 16) {
        unsafe {
            f0 = _mm256_load_si256(&a.vec[i]);
            f1 = _mm256_load_si256(&r.vec[i]);
            f0 = _mm256_sub_epi16(f0, f1);
            _mm256_store_si256(&mut r.vec[i], f0);
        }
    }
}
