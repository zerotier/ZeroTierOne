
#![allow(non_snake_case, dead_code)]
use core::arch::x86_64::*;
use crate::params::KYBER_N;
use crate::poly::*;
use crate::align::Eta4xBuf;
#[cfg(feature="90s")]
use crate::align::IndcpaBuf;

fn cbd2(r: &mut Poly, buf: &[__m256i]) {
  unsafe {
    let mask55: __m256i = _mm256_set1_epi32(0x55555555);
    let mask33: __m256i = _mm256_set1_epi32(0x33333333);
    let mask03: __m256i = _mm256_set1_epi32(0x03030303);
    let mask0F: __m256i = _mm256_set1_epi32(0x0F0F0F0F);
    let (mut f0, mut f1, mut f2, mut f3); 
    for i in 0..(KYBER_N/64) {
      f0 = _mm256_load_si256(&buf[i]);

      f1 = _mm256_srli_epi16(f0, 1);
      f0 = _mm256_and_si256(mask55, f0);
      f1 = _mm256_and_si256(mask55, f1);
      f0 = _mm256_add_epi8(f0, f1);

      f1 = _mm256_srli_epi16(f0, 2);
      f0 = _mm256_and_si256(mask33, f0);
      f1 = _mm256_and_si256(mask33, f1);
      f0 = _mm256_add_epi8(f0, mask33);
      f0 = _mm256_sub_epi8(f0, f1);

      f1 = _mm256_srli_epi16(f0, 4);
      f0 = _mm256_and_si256(mask0F, f0);
      f1 = _mm256_and_si256(mask0F, f1);
      f0 = _mm256_sub_epi8(f0, mask03);
      f1 = _mm256_sub_epi8(f1, mask03);

      f2 = _mm256_unpacklo_epi8(f0, f1);
      f3 = _mm256_unpackhi_epi8(f0, f1);

      f0 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(f2));
      f1 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(f2,1));
      f2 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(f3));
      f3 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(f3,1));

      _mm256_store_si256(&mut r.vec[4*i+0], f0);
      _mm256_store_si256(&mut r.vec[4*i+1], f2);
      _mm256_store_si256(&mut r.vec[4*i+2], f1);
      _mm256_store_si256(&mut r.vec[4*i+3], f3);
    }
  }
}

fn cbd3(r: &mut Poly, buf: &[u8]) {
  unsafe {
    let (mut f0, mut f1, mut f2, mut f3);
    let mask249: __m256i = _mm256_set1_epi32(0x249249);
    let mask6DB: __m256i = _mm256_set1_epi32(0x6DB6DB);
    let mask07: __m256i = _mm256_set1_epi32(7);
    let mask70: __m256i = _mm256_set1_epi32(7 << 16);
    let mask: __m256i = _mm256_set1_epi16(3);
    let shufbidx: __m256i = _mm256_set_epi8(
      -1,15,14,13,-1,12,11,10,-1, 9, 8, 7,-1, 6, 5, 4,
      -1,11,10, 9,-1, 8, 7, 6,-1, 5, 4, 3,-1, 2, 1, 0
    );

    for i in 0..(KYBER_N/32) {
      f0 = _mm256_loadu_si256(buf[24*i..].as_ptr() as *const __m256i);
      f0 = _mm256_permute4x64_epi64(f0,0x94);
      f0 = _mm256_shuffle_epi8(f0,shufbidx);

      f1 = _mm256_srli_epi32(f0,1);
      f2 = _mm256_srli_epi32(f0,2);
      f0 = _mm256_and_si256(mask249,f0);
      f1 = _mm256_and_si256(mask249,f1);
      f2 = _mm256_and_si256(mask249,f2);
      f0 = _mm256_add_epi32(f0,f1);
      f0 = _mm256_add_epi32(f0,f2);

      f1 = _mm256_srli_epi32(f0,3);
      f0 = _mm256_add_epi32(f0,mask6DB);
      f0 = _mm256_sub_epi32(f0,f1);

      f1 = _mm256_slli_epi32(f0,10);
      f2 = _mm256_srli_epi32(f0,12);
      f3 = _mm256_srli_epi32(f0, 2);
      f0 = _mm256_and_si256(f0,mask07);
      f1 = _mm256_and_si256(f1,mask70);
      f2 = _mm256_and_si256(f2,mask07);
      f3 = _mm256_and_si256(f3,mask70);
      f0 = _mm256_add_epi16(f0,f1);
      f1 = _mm256_add_epi16(f2,f3);
      f0 = _mm256_sub_epi16(f0,mask);
      f1 = _mm256_sub_epi16(f1,mask);

      f2 = _mm256_unpacklo_epi32(f0,f1);
      f3 = _mm256_unpackhi_epi32(f0,f1);

      f0 = _mm256_permute2x128_si256(f2,f3,0x20);
      f1 = _mm256_permute2x128_si256(f2,f3,0x31);

      _mm256_store_si256(&mut r.vec[2*i+0], f0);
      _mm256_store_si256(&mut r.vec[2*i+1], f1);
    }
  }
}

pub fn poly_cbd_eta1(r: &mut Poly, buf: &Eta4xBuf) 
{
  unsafe {
    if cfg!(feature="kyber512") {
      cbd3(r, &buf.coeffs)
    } 
    else {
      cbd2(r, &buf.vec)
    }
  }
}

#[cfg(feature="90s")]
pub fn poly_cbd_eta1_90s(r: &mut Poly, buf: &IndcpaBuf) 
{
  unsafe {
    if cfg!(feature="kyber512") {
      cbd3(r, &buf.coeffs)
    } 
    else {
      cbd2(r, &buf.vec)
    }
  }
}

 
pub fn poly_cbd_eta2(r: &mut Poly, buf: &[__m256i]) 
{
  cbd2(r, &buf)
}