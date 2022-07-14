#![allow(dead_code)]

use core::arch::x86_64::*;
use crate::fips202::*;
use crate::keccak4x::f1600_x4;
use crate::align::{GenMatrixBuf, Eta4xBuf};

#[repr(C)]
pub struct Keccakx4State {
  s: [__m256i; 25]
}

impl Keccakx4State {
  pub fn new() -> Self {
    unsafe {Keccakx4State { s: [_mm256_setzero_si256(); 25]}}
  }
}

pub unsafe fn keccakx4_absorb_once(
  s: &mut[__m256i; 25],
  r: usize,
  in0: &[u8],
  in1: &[u8],
  in2: &[u8],
  in3: &[u8],
  mut inlen: usize,
  p: u8
)
{
  let mut pos = 0i64;
  let mut t;
  for i in 0..25 {
    s[i] = _mm256_setzero_si256();
  }
  let mut idx = _mm256_set_epi64x(
    in3.as_ptr() as i64,
    in2.as_ptr() as i64,
    in1.as_ptr() as i64,
    in0.as_ptr() as i64,
  );
  while inlen >= r {
    for i in 0..(r/8) {
      t = _mm256_i64gather_epi64(pos as *const i64, idx, 1);
      s[i] = _mm256_xor_si256(s[i], t);
      pos += 8;
    }
    inlen -= r;
    f1600_x4(s);
  }
  let end = inlen/8;
  for i in 0..end {
    t = _mm256_i64gather_epi64(pos as *const i64, idx, 1);
    s[i] = _mm256_xor_si256(s[i], t);
    pos += 8;
  }
  inlen -= 8*end;

  if inlen > 0 {
    t = _mm256_i64gather_epi64(pos  as *const i64, idx, 1);
    idx = _mm256_set1_epi64x(((1u64 << (8*inlen)) - 1) as i64);
    t = _mm256_and_si256(t, idx);
    s[end] = _mm256_xor_si256(s[end], t);
  }

  t = _mm256_set1_epi64x(((p as u64) << 8*inlen) as i64);
  s[end] = _mm256_xor_si256(s[end], t);
  t = _mm256_set1_epi64x((1u64 << 63) as i64);
  s[r/8 - 1] = _mm256_xor_si256(s[r/8 - 1], t);
}

pub unsafe fn keccakx4_squeezeblocks128(
  out: &mut [GenMatrixBuf; 4],
  mut nblocks: usize,
  r: usize,
  s: &mut [__m256i; 25]
)
{
  let mut t;
  let mut idx = 0usize;
  while nblocks > 0 {
    f1600_x4(s);
    for i in 0..(r/8) {
      t = _mm_castsi128_pd(_mm256_castsi256_si128(s[i]));
      let out0_ptr = out[0].coeffs[idx+8*i..].as_mut_ptr();
      let out1_ptr = out[1].coeffs[idx+8*i..].as_mut_ptr();
      _mm_storel_pd(out0_ptr as *mut f64, t);
      _mm_storeh_pd(out1_ptr as *mut f64, t);
      
      t = _mm_castsi128_pd(_mm256_extracti128_si256(s[i],1));
      let out2_ptr = out[2].coeffs[idx+8*i..].as_mut_ptr();
      let out3_ptr = out[3].coeffs[idx+8*i..].as_mut_ptr();
      _mm_storel_pd(out2_ptr as *mut f64, t);
      _mm_storeh_pd(out3_ptr as *mut f64, t);
    }
    idx += r;
    nblocks -= 1;
  }
}

pub unsafe fn keccakx4_squeezeblocks256(
  out: &mut [Eta4xBuf; 4],
  mut nblocks: usize,
  r: usize,
  s: &mut [__m256i; 25]
)
{
  let mut t;
  let mut idx = 0usize;
  while nblocks > 0 {
    f1600_x4(s);
    for i in 0..(r/8) {
      t = _mm_castsi128_pd(_mm256_castsi256_si128(s[i]));
      _mm_storel_pd(out[0].coeffs[idx+8*i..].as_mut_ptr() as *mut f64, t);
      _mm_storeh_pd(out[1].coeffs[idx+8*i..].as_mut_ptr() as *mut f64, t);
      t = _mm_castsi128_pd(_mm256_extracti128_si256(s[i],1));
      _mm_storel_pd(out[2].coeffs[idx+8*i..].as_mut_ptr() as *mut f64, t);
      _mm_storeh_pd(out[3].coeffs[idx+8*i..].as_mut_ptr() as *mut f64, t);
    }
    idx += r;
    nblocks -= 1;
  }
}

// pub unsafe fn keccakx4_squeezeonce128(
//   out: &mut [[u8; 168]; 4],
//   s: &mut [__m256i; 25]
// )
// {
//   let mut t;
//   f1600_x4(s);
//   for i in 0..(SHAKE128_RATE/8) {
//     t = _mm_castsi128_pd(_mm256_castsi256_si128(s[i]));
//     let out0_ptr = out[0][8*i] as *mut f64;
//     _mm_storel_pd(out0_ptr, t);
//     _mm_storeh_pd(out[1][8*i] as *mut f64, t);
//     t = _mm_castsi128_pd(_mm256_extracti128_si256(s[i],1));
//     _mm_storel_pd(out[2][8*i] as *mut f64, t);
//     _mm_storeh_pd(out[3][8*i] as *mut f64, t);
//   }
// }

// pub unsafe fn keccakx4_squeezeonce256(
//   out: &mut [[u8; 136]; 4],
//   s: &mut [__m256i; 25]
// )
// {
//   let mut t;
//   f1600_x4(s);
//   for i in 0..(SHAKE256_RATE/8) {
//     t = _mm_castsi128_pd(_mm256_castsi256_si128(s[i]));
//     _mm_storel_pd(out[0][8*i] as *mut f64, t);
//     _mm_storeh_pd(out[1][8*i] as *mut f64, t);
//     t = _mm_castsi128_pd(_mm256_extracti128_si256(s[i],1));
//     _mm_storel_pd(out[2][8*i] as *mut f64, t);
//     _mm_storeh_pd(out[3][8*i] as *mut f64, t);
//   }
// }

pub unsafe fn shake128x4_absorb_once(
  state: &mut Keccakx4State,
  in0: &[u8],
  in1: &[u8],
  in2: &[u8],
  in3: &[u8],
  inlen: usize,
)
{
  keccakx4_absorb_once(
    &mut state.s, 
    SHAKE128_RATE, 
    in0, in1, in2, in3, inlen, 
    0x1F
  )
}

pub unsafe fn shake128x4_squeezeblocks(
  out: &mut[GenMatrixBuf; 4], 
  nblocks: usize,
  state: &mut Keccakx4State
)
{
  keccakx4_squeezeblocks128(
    out, 
    nblocks, 
    SHAKE128_RATE, 
    &mut state.s
  );
}

pub unsafe fn shake256x4_absorb_once(
  state: &mut Keccakx4State,
  in0: &[u8],
  in1: &[u8],
  in2: &[u8],
  in3: &[u8],
  inlen: usize,
)
{
  keccakx4_absorb_once(
    &mut state.s, 
    SHAKE256_RATE, 
    in0, in1, in2, in3, inlen, 
    0x1F
  )
}

pub unsafe fn shake256x4_squeezeblocks(
  out: &mut[Eta4xBuf; 4],
  nblocks: usize,
  state: &mut Keccakx4State
)
{
  keccakx4_squeezeblocks256(
    out, 
    nblocks, 
    SHAKE256_RATE, 
    &mut state.s
  );
}

// pub unsafe fn shake128x4(
//   out: &mut [GenMatrixBuf; 4],
//   mut outlen: usize,
//   in0: &[u8],
//   in1: &[u8],
//   in2: &[u8],
//   in3: &[u8],
//   inlen: usize
// )
// {
//   let nblocks = outlen/SHAKE128_RATE;
//   let mut t = [[0u8; SHAKE128_RATE]; 4];
//   let mut state = Keccakx4State::new();

//   shake128x4_absorb_once(&mut state, in0, in1, in2, in3, inlen);
//   shake128x4_squeezeblocks(out, nblocks, &mut state);
//   let idx = nblocks*SHAKE128_RATE; 
//   outlen -= idx;

//   if outlen > 0 {
//     keccakx4_squeezeonce128(&mut t, &mut state.s);
//     for i in 0..outlen {
//       out[0].coeffs[idx+i] = t[0][i];
//       out[1].coeffs[idx+i] = t[1][i];
//       out[2].coeffs[idx+i] = t[2][i];
//       out[3].coeffs[idx+i] = t[3][i];
//     }
//   }
// }

// pub unsafe fn shake256x4(
//   out: &mut [Eta4xBuf; 4],
//   mut outlen: usize,
//   in0: &[u8],
//   in1: &[u8],
//   in2: &[u8],
//   in3: &[u8],
//   inlen: usize
// )
// {
//   let nblocks = outlen/SHAKE256_RATE;
//   let mut t = [[0u8; SHAKE256_RATE] ; 4];
//   let mut state = Keccakx4State::new();

//   shake256x4_absorb_once(&mut state, in0, in1, in2, in3, inlen);
//   shake256x4_squeezeblocks(out, nblocks, &mut state);

//   let idx = nblocks*SHAKE256_RATE;
//   outlen -= nblocks*SHAKE256_RATE;

//   if outlen > 0 {
//     keccakx4_squeezeonce256(&mut t, &mut state.s);
//     for i in 0..outlen {
//       out[0].coeffs[idx+i] = t[0][i];
//       out[1].coeffs[idx+i] = t[1][i];
//       out[2].coeffs[idx+i] = t[2][i];
//       out[3].coeffs[idx+i] = t[3][i];
//     }
//   }
// }