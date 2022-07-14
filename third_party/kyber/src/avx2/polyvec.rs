use core::arch::x86_64::*;
use crate::{
  poly::*,
  params::*,
  consts::*
};

#[derive(Clone)]
pub struct Polyvec {
  pub vec: [Poly; KYBER_K]
}

impl Copy for Polyvec {}

impl Polyvec {
  pub fn new() -> Self {
    Polyvec {
      vec: [Poly::new(); KYBER_K]
    }
  }
  // Basic polynomial value check for development
  // #[cfg(debug_assertions)]
  // pub fn checksum(&self) -> i16 {
  //   unsafe {
  //     let mut out = 0i16;
  //     for i in 0..KYBER_K {
  //       for j in 0..KYBER_N {
  //         out ^= &self.vec[i].coeffs[j];
  //       }
  //     }
  //     out
  //   }
  // }
}

// #[target_feature(enable = "avx")]
pub unsafe fn poly_compress10(r: &mut[u8], a: &Poly)
{
  let (mut f0, mut f1, mut f2);
  let (mut t0, mut t1);

  let v = _mm256_load_si256(QDATA.vec[_16XV/16..].as_ptr());
  let v8 = _mm256_slli_epi16(v,3);
  let off = _mm256_set1_epi16(15);
  let shift1 = _mm256_set1_epi16(1 << 12);
  let mask = _mm256_set1_epi16(1023);
  let shift2 = _mm256_set1_epi64x(
    ((1024u64 << 48) + (1u64 << 32) + (1024 << 16) + 1) as i64
  );
  let sllvdidx = _mm256_set1_epi64x(12);
  let shufbidx = _mm256_set_epi8(
                  8, 4, 3, 2, 1, 0,-1,-1,-1,-1,-1,-1,12,11,10, 9,
                 -1,-1,-1,-1,-1,-1,12,11,10, 9, 8, 4, 3, 2, 1, 0
                );

  for i in 0..(KYBER_N/16) {
    f0 = _mm256_load_si256(&a.vec[i]);
    f1 = _mm256_mullo_epi16(f0,v8);
    f2 = _mm256_add_epi16(f0,off);
    f0 = _mm256_slli_epi16(f0,3);
    f0 = _mm256_mulhi_epi16(f0,v);
    f2 = _mm256_sub_epi16(f1,f2);
    f1 = _mm256_andnot_si256(f1,f2);
    f1 = _mm256_srli_epi16(f1,15);
    f0 = _mm256_sub_epi16(f0,f1);
    f0 = _mm256_mulhrs_epi16(f0,shift1);
    f0 = _mm256_and_si256(f0,mask);
    f0 = _mm256_madd_epi16(f0,shift2);
    f0 = _mm256_sllv_epi32(f0,sllvdidx);
    f0 = _mm256_srli_epi64(f0,12);
    f0 = _mm256_shuffle_epi8(f0,shufbidx);
    t0 = _mm256_castsi256_si128(f0);
    t1 = _mm256_extracti128_si256(f0,1);
    t0 = _mm_blend_epi16(t0,t1,0xE0);
    _mm_storeu_si128(r[20*i..].as_mut_ptr() as *mut __m128i,t0);
    _mm_storeu_si128(r[20*i+16..].as_mut_ptr()  as *mut __m128i, t1);
  }
}

// #[target_feature(enable = "avx")]
pub unsafe fn poly_decompress10(r: &mut Poly, a: &[u8])
{
  let mut f;
  let q = _mm256_set1_epi32(((KYBER_Q as i32) << 16) + 4*KYBER_Q as i32);
  let shufbidx = _mm256_set_epi8(11,10,10, 9, 9, 8, 8, 7,
                                  6, 5, 5, 4, 4, 3, 3, 2,
                                  9, 8, 8, 7, 7, 6, 6, 5,
                                  4, 3, 3, 2, 2, 1, 1, 0);
  let sllvdidx = _mm256_set1_epi64x(4);
  let mask = _mm256_set1_epi32((32736 << 16) + 8184);
  for i in 0..KYBER_N/16 {
    f = _mm256_loadu_si256(a[20*i..].as_ptr() as *const __m256i);
    f = _mm256_permute4x64_epi64(f,0x94);
    f = _mm256_shuffle_epi8(f,shufbidx);
    f = _mm256_sllv_epi32(f,sllvdidx);
    f = _mm256_srli_epi16(f,1);
    f = _mm256_and_si256(f,mask);
    f = _mm256_mulhrs_epi16(f,q);
    _mm256_store_si256(&mut r.vec[i],f);
  }
}

// #[target_feature(enable = "avx")]
pub unsafe fn poly_compress11(r: &mut[u8], a: &Poly)
{
  let (mut f0, mut f1, mut f2);
  let (mut t0, mut t1);
  let v = _mm256_load_si256(QDATA.vec[_16XV/16..].as_ptr());
  let v8 = _mm256_slli_epi16(v,3);
  let off = _mm256_set1_epi16(36);
  let shift1 = _mm256_set1_epi16(1 << 13);
  let mask = _mm256_set1_epi16(2047);
  let shift2 = _mm256_set1_epi64x(
    ((2048u64 << 48) + (1u64 << 32) + (2048 << 16) + 1) as i64
  );
  let sllvdidx = _mm256_set1_epi64x(10);
  let srlvqidx = _mm256_set_epi64x(30,10,30,10);
  let shufbidx = _mm256_set_epi8( 
    4, 3, 2, 1, 0, 0,-1,-1,-1,-1,10, 9, 8, 7, 6, 5,
    -1,-1,-1,-1,-1,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
  );

  for i in 0..KYBER_N/16 {
    f0 = _mm256_load_si256(&a.vec[i]);
    f1 = _mm256_mullo_epi16(f0,v8);
    f2 = _mm256_add_epi16(f0,off);
    f0 = _mm256_slli_epi16(f0,3);
    f0 = _mm256_mulhi_epi16(f0,v);
    f2 = _mm256_sub_epi16(f1,f2);
    f1 = _mm256_andnot_si256(f1,f2);
    f1 = _mm256_srli_epi16(f1,15);
    f0 = _mm256_sub_epi16(f0,f1);
    f0 = _mm256_mulhrs_epi16(f0,shift1);
    f0 = _mm256_and_si256(f0,mask);
    f0 = _mm256_madd_epi16(f0,shift2);
    f0 = _mm256_sllv_epi32(f0,sllvdidx);
    f1 = _mm256_bsrli_epi128(f0,8);
    f0 = _mm256_srlv_epi64(f0,srlvqidx);
    f1 = _mm256_slli_epi64(f1,34);
    f0 = _mm256_add_epi64(f0,f1);
    f0 = _mm256_shuffle_epi8(f0,shufbidx);
    t0 = _mm256_castsi256_si128(f0);
    t1 = _mm256_extracti128_si256(f0,1);
    t0 = _mm_blendv_epi8(t0,t1,_mm256_castsi256_si128(shufbidx));
    _mm_storeu_si128(r[22*i+ 0..].as_mut_ptr() as *mut __m128i,t0);
    _mm_storel_epi64(r[22*i+16..].as_mut_ptr() as *mut __m128i,t1);
  }
}

// #[target_feature(enable = "avx")]
pub unsafe fn poly_decompress11(r: &mut Poly, a: &[u8])
{
  let mut f;

  let q = _mm256_load_si256(QDATA.vec[_16XQ/16..].as_ptr());
  let shufbidx = _mm256_set_epi8(
                    13,12,12,11,10, 9, 9, 8,
                    8, 7, 6, 5, 5, 4, 4, 3,
                    10, 9, 9, 8, 7, 6, 6, 5,
                    5, 4, 3, 2, 2, 1, 1, 0
                  );
  let srlvdidx = _mm256_set_epi32(0,0,1,0,0,0,1,0);
  let srlvqidx = _mm256_set_epi64x(2,0,2,0);
  let shift = _mm256_set_epi16(4,32,1,8,32,1,4,32,4,32,1,8,32,1,4,32);
  let mask = _mm256_set1_epi16(32752);
  
  for i in 0..(KYBER_N/16) {
    f = _mm256_loadu_si256(a[22*i..].as_ptr() as *const __m256i);
    f = _mm256_permute4x64_epi64(f,0x94);
    f = _mm256_shuffle_epi8(f,shufbidx);
    f = _mm256_srlv_epi32(f,srlvdidx);
    f = _mm256_srlv_epi64(f,srlvqidx);
    f = _mm256_mullo_epi16(f,shift);
    f = _mm256_srli_epi16(f,1);
    f = _mm256_and_si256(f,mask);
    f = _mm256_mulhrs_epi16(f,q);
    _mm256_store_si256(&mut r.vec[i],f);
  }
}


pub unsafe fn polyvec_compress(r: &mut[u8], a: &Polyvec)
{
  if cfg!(feature="kyber1024") {
    for i in 0..KYBER_K {
      poly_compress11(&mut r[352*i..], &a.vec[i]);
    } 
  } else {
    for i in 0..KYBER_K {
      poly_compress10(&mut r[320*i..], &a.vec[i]);
    }
  }
}

pub unsafe fn polyvec_decompress(r: &mut Polyvec, a: &[u8])
{
  if cfg!(feature="kyber1024") {
    for i in 0..KYBER_K {
      poly_decompress11(&mut r.vec[i], &a[352*i..]);
    } 
  } else {
    for i in 0..KYBER_K {
      poly_decompress10(&mut r.vec[i], &a[320*i..]);
    }
  }
}

pub fn polyvec_tobytes(r: &mut[u8], a: &Polyvec)
{
  for i in 0..KYBER_K {
    poly_tobytes(&mut r[i*KYBER_POLYBYTES..], a.vec[i]);
  }
}

pub unsafe fn polyvec_frombytes(r: &mut Polyvec, a: &[u8])
{
  for i in 0..KYBER_K {
    poly_frombytes(&mut r.vec[i], &a[i*KYBER_POLYBYTES..]);
  }
}

// Name:        polyvec_ntt
//
// Description: Apply forward NTT to all elements of a vector of polynomials
//
// Arguments:   - Polyvec r: in/output vector of polynomials
pub fn polyvec_ntt(r: &mut Polyvec)
{
  for i in 0..KYBER_K {
    poly_ntt(&mut r.vec[i]);
  }
}

// Name:        polyvec_invntt
//
// Description: Apply inverse NTT to all elements of a vector of polynomials
//
// Arguments:   - Polyvec r: in/output vector of polynomials
pub fn polyvec_invntt_tomont(r: &mut Polyvec)
{
  for i in 0..KYBER_K {
    poly_invntt_tomont(&mut r.vec[i]);
  }
}

// Name:        polyvec_basemul_acc_montgomery
//
// Description: Pointwise multiply elements of a and b and accumulate into r
//
// Arguments: - poly *r:          output polynomial
//            - const Polyvec a: first input vector of polynomials
//            - const Polyvec b: second input vector of polynomials
pub fn polyvec_basemul_acc_montgomery(r: &mut Poly, a: &Polyvec, b: &Polyvec)
{
  let mut t = Poly::new();
  poly_basemul(r, &a.vec[0], &b.vec[0]);
  for i in 1..KYBER_K {
    poly_basemul(&mut t, &a.vec[i], &b.vec[i]);
    poly_add(r, &t);
  }
}

// Name:        polyvec_reduce
//
// Description: Applies Barrett reduction to each coefficient 
//              of each element of a vector of polynomials
//              for details of the Barrett reduction see comments in reduce.c
//
// Arguments:   - poly *r:       input/output polynomial
pub fn polyvec_reduce(r: &mut Polyvec)
{
 for i in 0..KYBER_K {
  poly_reduce(&mut r.vec[i]);
 } 
}


// Name:        polyvec_add
//
// Description: Add vectors of polynomials
//
// Arguments: - Polyvec r:       output vector of polynomials
//            - const Polyvec a: first input vector of polynomials
//            - const Polyvec b: second input vector of polynomials
pub fn polyvec_add(r: &mut Polyvec, b: &Polyvec)
{
  for i in 0..KYBER_K {
    poly_add(&mut r.vec[i], &b.vec[i]);
  }
}
