// Based heavily on public-domain code by Romain Dolbeau
// Different handling of nonce+counter than original version using
// separated 64-bit nonce and internal 64-bit counter, starting from zero
// Public Domain 
#![cfg(feature="90s")]

use core::arch::x86_64::*;

#[derive(Clone, Copy)]
#[repr(C)]
pub(crate) struct Aes256CtrCtx {
  pub rkeys: [__m128i; 16],
  pub n: __m128i
}

impl Aes256CtrCtx {
  pub fn new() -> Self {
    unsafe {
      Self {
        rkeys: [_mm_setzero_si128(); 16],
        n: _mm_setzero_si128()
      }
    }
  }
}

unsafe fn aesni_encrypt4(out: &mut[u8], n :&mut __m128i, rkeys: &[__m128i; 16]) 
{
  let idx: __m128i = _mm_set_epi8(8,9,10,11,12,13,14,15,7,6,5,4,3,2,1,0);

  // Load current counter value
  let mut f = _mm_load_si128(n);

  // Increase counter in 4 consecutive blocks
  let mut f0 = _mm_shuffle_epi8(_mm_add_epi64(f,_mm_set_epi64x(0,0)),idx);
  let mut f1 = _mm_shuffle_epi8(_mm_add_epi64(f,_mm_set_epi64x(1,0)),idx);
  let mut f2 = _mm_shuffle_epi8(_mm_add_epi64(f,_mm_set_epi64x(2,0)),idx);
  let mut f3 = _mm_shuffle_epi8(_mm_add_epi64(f,_mm_set_epi64x(3,0)),idx);

  // Write counter for next iteration, increased by 4 
  _mm_store_si128(n as *mut __m128i,_mm_add_epi64(f,_mm_set_epi64x(4,0)));

  // Actual AES encryption, 4x interleaved4
  f  = _mm_load_si128(&rkeys[0]);
  f0 = _mm_xor_si128(f0,f);
  f1 = _mm_xor_si128(f1,f);
  f2 = _mm_xor_si128(f2,f);
  f3 = _mm_xor_si128(f3,f);

  for i in 1..14 {
    f  = _mm_load_si128(&rkeys[i]);
    f0 = _mm_aesenc_si128(f0,f);
    f1 = _mm_aesenc_si128(f1,f);
    f2 = _mm_aesenc_si128(f2,f);
    f3 = _mm_aesenc_si128(f3,f);
  }

  f  = _mm_load_si128(&rkeys[14]);
  f0 = _mm_aesenclast_si128(f0,f);
  f1 = _mm_aesenclast_si128(f1,f);
  f2 = _mm_aesenclast_si128(f2,f);
  f3 = _mm_aesenclast_si128(f3,f);

  // Write results
  _mm_storeu_si128(out[..].as_mut_ptr() as *mut __m128i, f0);
  _mm_storeu_si128(out[16..].as_mut_ptr() as *mut __m128i, f1);
  _mm_storeu_si128(out[32..].as_mut_ptr() as *mut __m128i, f2);
  _mm_storeu_si128(out[48..].as_mut_ptr() as *mut __m128i, f3);
}

// Casting aliases
unsafe fn cast_128i(x: __m128) -> __m128i
{
  _mm_castps_si128(x)
}

unsafe fn cast_128(x: __m128i) -> __m128
{
  _mm_castsi128_ps(x)
}

pub(crate) fn aes256ctr_init(state: &mut Aes256CtrCtx, key: &[u8], nonce: [u8; 12])
{
  unsafe {
    let mut idx = 0;
    let key0 = _mm_loadu_si128(key.as_ptr() as *const __m128i);
    let key1 = _mm_loadu_si128(key[16..].as_ptr() as *const __m128i);

    state.n = _mm_loadl_epi64(nonce[..].as_ptr() as *const __m128i);
    state.rkeys[idx] = key0;
    idx += 1;
    let mut temp0 = key0;
    let mut temp1;
    let mut temp2 = key1;
    let mut temp4 = _mm_setzero_si128();
  
    macro_rules! block1  {
      ($imm:expr) => {
        temp1 = _mm_aeskeygenassist_si128(temp2, $imm);
        state.rkeys[idx] = temp2;
        idx += 1;
        temp4 =  cast_128i(_mm_shuffle_ps(cast_128(temp4), cast_128(temp0), 0x10));
        temp0 = _mm_xor_si128(temp0, temp4);
        temp4 = cast_128i(_mm_shuffle_ps(cast_128(temp4), cast_128(temp0), 0x8c));
        temp0 = _mm_xor_si128(temp0, temp4);
        temp1 = cast_128i(_mm_shuffle_ps(cast_128(temp1), cast_128(temp1), 0xff));
        temp0 = _mm_xor_si128(temp0, temp1)
      };
    }

    macro_rules! block2  {
      ($imm:expr) => {
        temp1 = _mm_aeskeygenassist_si128(temp0, $imm);
        state.rkeys[idx] = temp0;
        idx += 1;
        temp4 = cast_128i(_mm_shuffle_ps(cast_128(temp4), cast_128(temp2), 0x10));
        temp2 = _mm_xor_si128(temp2, temp4);
        temp4 = cast_128i(_mm_shuffle_ps(cast_128(temp4), cast_128(temp2), 0x8c));
        temp2 = _mm_xor_si128(temp2, temp4);
        temp1 = cast_128i(_mm_shuffle_ps(cast_128(temp1), cast_128(temp1), 0xaa));
        temp2 = _mm_xor_si128(temp2, temp1)
      };
    } 

    block1!(0x01);
    block2!(0x01);
    block1!(0x02);
    block2!(0x02);

    block1!(0x04);
    block2!(0x04);
    block1!(0x08);
    block2!(0x08);

    block1!(0x10);
    block2!(0x10);
    block1!(0x20);
    block2!(0x20);

    block1!(0x40);
    state.rkeys[idx] = temp0;
  }
}

pub(crate) fn aes256ctr_squeezeblocks(out: &mut[u8], nblocks: usize, state: &mut Aes256CtrCtx)
{
  let mut idx = 0;
  for _ in 0..nblocks {
    unsafe { aesni_encrypt4(&mut out[idx..], &mut state.n, &state.rkeys); }
    idx += 64
  }
}

#[cfg(feature="90s")]
pub(crate) fn aes256ctr_prf(out: &mut[u8], mut outlen: usize, seed: &[u8], nonce: u8)
{
  let mut buf = [0u8; 64];
  let mut idx = 0;
  let mut pad_nonce = [0u8; 12];
  let mut state = unsafe{
    Aes256CtrCtx{rkeys: [ _mm_setzero_si128(); 16], n: _mm_setzero_si128()}
  };
  
  pad_nonce[0] = nonce;
  aes256ctr_init(&mut state, seed, pad_nonce);  
  
  while outlen >= 64 {
    unsafe { aesni_encrypt4(&mut out[idx..], &mut state.n, &state.rkeys); }
    outlen -= 64;
    idx += 64;
  }

  if outlen != 0 {
    unsafe { aesni_encrypt4(&mut buf, &mut state.n, &state.rkeys); }
    out[idx..][..outlen].copy_from_slice(&buf[..outlen]);
  }
} 