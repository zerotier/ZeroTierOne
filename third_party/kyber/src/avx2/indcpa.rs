#[cfg(not(feature = "KATs"))]
use crate::rng::randombytes;
#[cfg(feature = "90s")]
use crate::{aes256ctr::*, cbd::*};
use crate::{align::*, params::*, poly::*, polyvec::*, rejsample::*, symmetric::*, CryptoRng, RngCore};
#[cfg(not(feature = "90s"))]
use crate::{fips202::*, fips202x4::*};
use core::arch::x86_64::*;

// Name:        pack_pk
//
// Description: Serialize the public key as concatenation of the
//              serialized vector of polynomials pk
//              and the public seed used to generate the matrix A.
//
// Arguments:   [u8] r:          the output serialized public key
//              const poly *pk:            the input public-key polynomial
//              const [u8] seed: the input public seed
fn pack_pk(r: &mut [u8], pk: &Polyvec, seed: &[u8]) {
    polyvec_tobytes(r, pk);
    r[KYBER_POLYVECBYTES..][..KYBER_SYMBYTES].copy_from_slice(&seed[..KYBER_SYMBYTES]);
}

// Name:        unpack_pk
//
// Description: De-serialize public key from a byte array;
//              approximate inverse of pack_pk
//
// Arguments:   - Polyvec pk:                   output public-key vector of polynomials
//              - [u8] seed:           output seed to generate matrix A
//              - const [u8] packedpk: input serialized public key
fn unpack_pk(pk: &mut Polyvec, seed: &mut [u8], packedpk: &[u8]) {
    unsafe {
        polyvec_frombytes(pk, packedpk);
    }
    seed[..KYBER_SYMBYTES].copy_from_slice(&packedpk[KYBER_POLYVECBYTES..][..KYBER_SYMBYTES]);
}

// Name:        pack_sk
//
// Description: Serialize the secret key
//
// Arguments:   - [u8] r:  output serialized secret key
//              - const Polyvec sk: input vector of polynomials (secret key)
fn pack_sk(r: &mut [u8], sk: &Polyvec) {
    polyvec_tobytes(r, sk);
}

// Name:        unpack_sk
//
// Description: De-serialize the secret key;
//              inverse of pack_sk
//
// Arguments:   - Polyvec sk:                   output vector of polynomials (secret key)
//              - const [u8] packedsk: input serialized secret key
fn unpack_sk(sk: &mut Polyvec, packedsk: &[u8]) {
    unsafe {
        polyvec_frombytes(sk, packedsk);
    }
}

// Name:        pack_ciphertext
//
// Description: Serialize the ciphertext as concatenation of the
//              compressed and serialized vector of polynomials b
//              and the compressed and serialized polynomial v
//
// Arguments:   [u8] r:          the output serialized ciphertext
//              const poly *pk:            the input vector of polynomials b
//              const [u8] seed: the input polynomial v
fn pack_ciphertext(r: &mut [u8], b: &Polyvec, v: Poly) {
    unsafe {
        polyvec_compress(r, b);
        poly_compress(&mut r[KYBER_POLYVECCOMPRESSEDBYTES..], v);
    }
}

// Name:        unpack_ciphertext
//
// Description: De-serialize and decompress ciphertext from a byte array;
//              approximate inverse of pack_ciphertext
//
// Arguments:   - Polyvec b:             output vector of polynomials b
//              - Poly *v:                output polynomial v
//              - const [u8] c:           input serialized ciphertext
fn unpack_ciphertext(b: &mut Polyvec, v: &mut Poly, c: &[u8]) {
    unsafe {
        polyvec_decompress(b, c);
        poly_decompress(v, &c[KYBER_POLYVECCOMPRESSEDBYTES..]);
    }
}

// Name:        rej_uniform
//
// Description: Run rejection sampling on uniform random bytes to generate
//              uniform random integers mod q
//
// Arguments:   - i16 *r:        output buffer
//              - usize len:         requested number of 16-bit integers (uniform mod q)
//              - const [u8] buf:    input buffer (assumed to be uniform random bytes)
//              - usize buflen:      length of input buffer in bytes
//
// Returns number of sampled 16-bit integers (at most len)
fn rej_uniform(r: &mut [i16], len: usize, buf: &[u8], buflen: usize) -> usize {
    let (mut ctr, mut pos) = (0usize, 0usize);
    let (mut val0, mut val1);

    while ctr < len && pos + 3 <= buflen {
        val0 = ((buf[pos + 0] >> 0) as u16 | (buf[pos + 1] as u16) << 8) & 0xFFF;
        val1 = ((buf[pos + 1] >> 4) as u16 | (buf[pos + 2] as u16) << 4) & 0xFFF;
        pos += 3;

        if val0 < KYBER_Q as u16 {
            r[ctr] = val0 as i16;
            ctr += 1;
        }
        if ctr < len && val1 < KYBER_Q as u16 {
            r[ctr] = val1 as i16;
            ctr += 1;
        }
    }
    ctr
}

pub fn gen_a(a: &mut [Polyvec], b: &[u8]) {
    unsafe {
        gen_matrix(a, b, false);
    }
}

pub fn gen_at(a: &mut [Polyvec], b: &[u8]) {
    unsafe {
        gen_matrix(a, b, true);
    }
}

#[cfg(feature = "90s")]
unsafe fn gen_matrix(a: &mut [Polyvec], seed: &[u8], transposed: bool) {
    let (mut ctr, mut off, mut buflen);
    let mut nonce: u64;
    let mut state = Aes256CtrCtx::new();
    let mut buf = GenMatrixBuf90s::new();
    aes256ctr_init(&mut state, seed, [0u8; 12]);
    for i in 0..KYBER_K {
        for j in 0..KYBER_K {
            if transposed {
                nonce = ((j << 8) | i) as u64;
            } else {
                nonce = ((i << 8) | j) as u64;
            }
            state.n = _mm_loadl_epi64([nonce].as_ptr() as *const __m128i);
            aes256ctr_squeezeblocks(&mut buf.coeffs, REJ_UNIFORM_AVX_NBLOCKS, &mut state);
            buflen = REJ_UNIFORM_AVX_NBLOCKS * XOF_BLOCKBYTES;
            ctr = rej_uniform_avx(&mut a[i].vec[j].coeffs, &buf.coeffs);
            while ctr < KYBER_N {
                off = buflen % 3;
                for k in 0..off {
                    buf.coeffs[k] = buf.coeffs[buflen - off + k];
                }
                aes256ctr_squeezeblocks(&mut buf.coeffs[off..], 1, &mut state);
                buflen = off + XOF_BLOCKBYTES;
                ctr += rej_uniform(&mut a[i].vec[j].coeffs[ctr..], KYBER_N - ctr, &buf.coeffs, buflen);
            }
            poly_nttunpack(&mut a[i].vec[j]);
        }
    }
}

#[cfg(all(feature = "kyber512", not(feature = "90s")))]
unsafe fn gen_matrix(a: &mut [Polyvec], seed: &[u8], transposed: bool) {
    let mut state = Keccakx4State::new();
    let mut buf = [GenMatrixBuf::new(); 4];

    let f = _mm256_loadu_si256(seed[..].as_ptr() as *const __m256i);
    _mm256_store_si256(buf[0].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[1].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[2].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[3].vec.as_mut_ptr(), f);

    if transposed {
        buf[0].coeffs[32] = 0;
        buf[0].coeffs[33] = 0;
        buf[1].coeffs[32] = 0;
        buf[1].coeffs[33] = 1;
        buf[2].coeffs[32] = 1;
        buf[2].coeffs[33] = 0;
        buf[3].coeffs[32] = 1;
        buf[3].coeffs[33] = 1;
    } else {
        buf[0].coeffs[32] = 0;
        buf[0].coeffs[33] = 0;
        buf[1].coeffs[32] = 1;
        buf[1].coeffs[33] = 0;
        buf[2].coeffs[32] = 0;
        buf[2].coeffs[33] = 1;
        buf[3].coeffs[32] = 1;
        buf[3].coeffs[33] = 1;
    }

    shake128x4_absorb_once(&mut state, &buf[0].coeffs, &buf[1].coeffs, &buf[2].coeffs, &buf[3].coeffs, 34);
    shake128x4_squeezeblocks(&mut buf, REJ_UNIFORM_AVX_NBLOCKS, &mut state);

    let mut ctr0 = rej_uniform_avx(&mut a[0].vec[0].coeffs, &buf[0].coeffs);
    let mut ctr1 = rej_uniform_avx(&mut a[0].vec[1].coeffs, &buf[1].coeffs);
    let mut ctr2 = rej_uniform_avx(&mut a[1].vec[0].coeffs, &buf[2].coeffs);
    let mut ctr3 = rej_uniform_avx(&mut a[1].vec[1].coeffs, &buf[3].coeffs);

    while ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N {
        shake128x4_squeezeblocks(&mut buf, 1, &mut state);

        ctr0 += rej_uniform(&mut a[0].vec[0].coeffs[ctr0..], KYBER_N - ctr0, &buf[0].coeffs, SHAKE128_RATE);
        ctr1 += rej_uniform(&mut a[0].vec[1].coeffs[ctr1..], KYBER_N - ctr1, &buf[1].coeffs, SHAKE128_RATE);
        ctr2 += rej_uniform(&mut a[1].vec[0].coeffs[ctr2..], KYBER_N - ctr2, &buf[2].coeffs, SHAKE128_RATE);
        ctr3 += rej_uniform(&mut a[1].vec[1].coeffs[ctr3..], KYBER_N - ctr3, &buf[3].coeffs, SHAKE128_RATE);
    }

    poly_nttunpack(&mut a[0].vec[0]);
    poly_nttunpack(&mut a[0].vec[1]);
    poly_nttunpack(&mut a[1].vec[0]);
    poly_nttunpack(&mut a[1].vec[1]);
}

#[cfg(all(not(feature = "kyber512"), not(feature = "kyber1024"), not(feature = "90s")))]
unsafe fn gen_matrix(a: &mut [Polyvec], seed: &[u8], transposed: bool) {
    let mut state = Keccakx4State::new();
    let mut state1x = KeccakState::new();
    let mut buf = [GenMatrixBuf::new(); 4];

    let mut f = _mm256_loadu_si256(seed.as_ptr() as *const __m256i);
    _mm256_store_si256(buf[0].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[1].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[2].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[3].vec.as_mut_ptr(), f);

    if transposed {
        buf[0].coeffs[32] = 0;
        buf[0].coeffs[33] = 0;
        buf[1].coeffs[32] = 0;
        buf[1].coeffs[33] = 1;
        buf[2].coeffs[32] = 0;
        buf[2].coeffs[33] = 2;
        buf[3].coeffs[32] = 1;
        buf[3].coeffs[33] = 0;
    } else {
        buf[0].coeffs[32] = 0;
        buf[0].coeffs[33] = 0;
        buf[1].coeffs[32] = 1;
        buf[1].coeffs[33] = 0;
        buf[2].coeffs[32] = 2;
        buf[2].coeffs[33] = 0;
        buf[3].coeffs[32] = 0;
        buf[3].coeffs[33] = 1;
    }

    shake128x4_absorb_once(&mut state, &buf[0].coeffs, &buf[1].coeffs, &buf[2].coeffs, &buf[3].coeffs, 34);
    shake128x4_squeezeblocks(&mut buf, REJ_UNIFORM_AVX_NBLOCKS, &mut state);

    let mut ctr0 = rej_uniform_avx(&mut a[0].vec[0].coeffs, &buf[0].coeffs);
    let mut ctr1 = rej_uniform_avx(&mut a[0].vec[1].coeffs, &buf[1].coeffs);
    let mut ctr2 = rej_uniform_avx(&mut a[0].vec[2].coeffs, &buf[2].coeffs);
    let mut ctr3 = rej_uniform_avx(&mut a[1].vec[0].coeffs, &buf[3].coeffs);

    while ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N {
        shake128x4_squeezeblocks(&mut buf, 1, &mut state);

        ctr0 += rej_uniform(&mut a[0].vec[0].coeffs[ctr0..], KYBER_N - ctr0, &buf[0].coeffs, SHAKE128_RATE);
        ctr1 += rej_uniform(&mut a[0].vec[1].coeffs[ctr1..], KYBER_N - ctr1, &buf[1].coeffs, SHAKE128_RATE);
        ctr2 += rej_uniform(&mut a[0].vec[2].coeffs[ctr2..], KYBER_N - ctr2, &buf[2].coeffs, SHAKE128_RATE);
        ctr3 += rej_uniform(&mut a[1].vec[0].coeffs[ctr3..], KYBER_N - ctr3, &buf[3].coeffs, SHAKE128_RATE);
    }

    poly_nttunpack(&mut a[0].vec[0]);
    poly_nttunpack(&mut a[0].vec[1]);
    poly_nttunpack(&mut a[0].vec[2]);
    poly_nttunpack(&mut a[1].vec[0]);

    f = _mm256_loadu_si256(seed.as_ptr() as *const __m256i);
    _mm256_store_si256(buf[0].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[1].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[2].vec.as_mut_ptr(), f);
    _mm256_store_si256(buf[3].vec.as_mut_ptr(), f);

    if transposed {
        buf[0].coeffs[32] = 1;
        buf[0].coeffs[33] = 1;
        buf[1].coeffs[32] = 1;
        buf[1].coeffs[33] = 2;
        buf[2].coeffs[32] = 2;
        buf[2].coeffs[33] = 0;
        buf[3].coeffs[32] = 2;
        buf[3].coeffs[33] = 1;
    } else {
        buf[0].coeffs[32] = 1;
        buf[0].coeffs[33] = 1;
        buf[1].coeffs[32] = 2;
        buf[1].coeffs[33] = 1;
        buf[2].coeffs[32] = 0;
        buf[2].coeffs[33] = 2;
        buf[3].coeffs[32] = 1;
        buf[3].coeffs[33] = 2;
    }

    shake128x4_absorb_once(&mut state, &buf[0].coeffs, &buf[1].coeffs, &buf[2].coeffs, &buf[3].coeffs, 34);
    shake128x4_squeezeblocks(&mut buf, REJ_UNIFORM_AVX_NBLOCKS, &mut state);

    ctr0 = rej_uniform_avx(&mut a[1].vec[1].coeffs, &buf[0].coeffs);
    ctr1 = rej_uniform_avx(&mut a[1].vec[2].coeffs, &buf[1].coeffs);
    ctr2 = rej_uniform_avx(&mut a[2].vec[0].coeffs, &buf[2].coeffs);
    ctr3 = rej_uniform_avx(&mut a[2].vec[1].coeffs, &buf[3].coeffs);

    while ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N {
        shake128x4_squeezeblocks(&mut buf, 1, &mut state);

        ctr0 += rej_uniform(&mut a[1].vec[1].coeffs[ctr0..], KYBER_N - ctr0, &buf[0].coeffs, SHAKE128_RATE);
        ctr1 += rej_uniform(&mut a[1].vec[2].coeffs[ctr1..], KYBER_N - ctr1, &buf[1].coeffs, SHAKE128_RATE);
        ctr2 += rej_uniform(&mut a[2].vec[0].coeffs[ctr2..], KYBER_N - ctr2, &buf[2].coeffs, SHAKE128_RATE);
        ctr3 += rej_uniform(&mut a[2].vec[1].coeffs[ctr3..], KYBER_N - ctr3, &buf[3].coeffs, SHAKE128_RATE);
    }

    poly_nttunpack(&mut a[1].vec[1]);
    poly_nttunpack(&mut a[1].vec[2]);
    poly_nttunpack(&mut a[2].vec[0]);
    poly_nttunpack(&mut a[2].vec[1]);

    f = _mm256_loadu_si256(seed.as_ptr() as *const __m256i);
    _mm256_store_si256(buf[0].vec.as_mut_ptr(), f);
    buf[0].coeffs[32] = 2;
    buf[0].coeffs[33] = 2;
    shake128_absorb_once(&mut state1x, &buf[0].coeffs, 34);
    shake128_squeezeblocks(&mut buf[0].coeffs, REJ_UNIFORM_AVX_NBLOCKS, &mut state1x);
    ctr0 = rej_uniform_avx(&mut a[2].vec[2].coeffs, &buf[0].coeffs);
    while ctr0 < KYBER_N {
        shake128_squeezeblocks(&mut buf[0].coeffs, 1, &mut state1x);
        ctr0 += rej_uniform(&mut a[2].vec[2].coeffs[ctr0..], KYBER_N - ctr0, &buf[0].coeffs, SHAKE128_RATE);
    }

    poly_nttunpack(&mut a[2].vec[2]);
}

#[cfg(all(feature = "kyber1024", not(feature = "90s")))]
unsafe fn gen_matrix(a: &mut [Polyvec], seed: &[u8], transposed: bool) {
    let mut f;
    let mut state = Keccakx4State::new();
    let mut buf = [GenMatrixBuf::new(); 4];

    for i in 0..4usize {
        f = _mm256_loadu_si256(seed[..].as_ptr() as *const __m256i);
        _mm256_store_si256(buf[0].coeffs.as_mut_ptr() as *mut __m256i, f);
        _mm256_store_si256(buf[1].coeffs.as_mut_ptr() as *mut __m256i, f);
        _mm256_store_si256(buf[2].coeffs.as_mut_ptr() as *mut __m256i, f);
        _mm256_store_si256(buf[3].coeffs.as_mut_ptr() as *mut __m256i, f);

        if transposed {
            for j in 0..4 {
                buf[j].coeffs[32] = i as u8;
                buf[j].coeffs[33] = j as u8;
            }
        } else {
            for j in 0..4 {
                buf[j].coeffs[32] = j as u8;
                buf[j].coeffs[33] = i as u8;
            }
        }

        shake128x4_absorb_once(&mut state, &buf[0].coeffs, &buf[1].coeffs, &buf[2].coeffs, &buf[3].coeffs, 34);
        shake128x4_squeezeblocks(&mut buf, REJ_UNIFORM_AVX_NBLOCKS, &mut state);

        let mut ctr0 = rej_uniform_avx(&mut a[i].vec[0].coeffs, &buf[0].coeffs);
        let mut ctr1 = rej_uniform_avx(&mut a[i].vec[1].coeffs, &buf[1].coeffs);
        let mut ctr2 = rej_uniform_avx(&mut a[i].vec[2].coeffs, &buf[2].coeffs);
        let mut ctr3 = rej_uniform_avx(&mut a[i].vec[3].coeffs, &buf[3].coeffs);

        while ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N {
            shake128x4_squeezeblocks(&mut buf, 1, &mut state);

            ctr0 += rej_uniform(&mut a[i].vec[0].coeffs[ctr0..], KYBER_N - ctr0, &buf[0].coeffs, SHAKE128_RATE);
            ctr1 += rej_uniform(&mut a[i].vec[1].coeffs[ctr1..], KYBER_N - ctr1, &buf[1].coeffs, SHAKE128_RATE);
            ctr2 += rej_uniform(&mut a[i].vec[2].coeffs[ctr2..], KYBER_N - ctr2, &buf[2].coeffs, SHAKE128_RATE);
            ctr3 += rej_uniform(&mut a[i].vec[3].coeffs[ctr3..], KYBER_N - ctr3, &buf[3].coeffs, SHAKE128_RATE);
        }

        poly_nttunpack(&mut a[i].vec[0]);
        poly_nttunpack(&mut a[i].vec[1]);
        poly_nttunpack(&mut a[i].vec[2]);
        poly_nttunpack(&mut a[i].vec[3]);
    }
}

pub fn indcpa_keypair<R>(pk: &mut [u8], sk: &mut [u8], _seed: Option<(&[u8], &[u8])>, _rng: &mut R)
where
    R: CryptoRng + RngCore,
{
    let mut a = [Polyvec::new(); KYBER_K];
    let (mut e, mut pkpv, mut skpv) = (Polyvec::new(), Polyvec::new(), Polyvec::new());
    let mut buf = [0u8; 2 * KYBER_SYMBYTES];
    let mut randbuf = [0u8; 2 * KYBER_SYMBYTES];

    #[cfg(not(feature = "KATs"))]
    randombytes(&mut randbuf, KYBER_SYMBYTES, _rng);

    #[cfg(feature = "KATs")]
    randbuf[..KYBER_SYMBYTES].copy_from_slice(&_seed.expect("KAT seed").0);

    hash_g(&mut buf, &randbuf, KYBER_SYMBYTES);

    let (publicseed, noiseseed) = buf.split_at(KYBER_SYMBYTES);
    gen_a(&mut a, publicseed);

    #[cfg(feature = "90s")]
    {
        // Assumes divisibility
        const NOISE_NBLOCKS: usize = (KYBER_ETA1 * KYBER_N / 4) / XOF_BLOCKBYTES;
        let mut nonce = 0u64;
        let mut state = Aes256CtrCtx::new();
        let mut coins = IndcpaBuf::new();
        aes256ctr_init(&mut state, noiseseed, [0u8; 12]);
        nonce += 1;
        unsafe {
            for i in 0..KYBER_K {
                aes256ctr_squeezeblocks(&mut coins.coeffs, NOISE_NBLOCKS, &mut state);
                state.n = _mm_loadl_epi64([nonce].as_ptr() as *const __m128i);
                nonce += 1;
                poly_cbd_eta1_90s(&mut skpv.vec[i], &coins);
            }
            for i in 0..KYBER_K {
                aes256ctr_squeezeblocks(&mut coins.coeffs, NOISE_NBLOCKS, &mut state);
                state.n = _mm_loadl_epi64([nonce].as_ptr() as *const __m128i);
                nonce += 1;
                poly_cbd_eta1_90s(&mut e.vec[i], &coins);
            }
        }
    }

    #[cfg(all(feature = "kyber512", not(feature = "90s")))]
    {
        let (skpv0, skpv1) = skpv.vec.split_at_mut(1);
        let (e0, e1) = e.vec.split_at_mut(1);
        poly_getnoise_eta1_4x(&mut skpv0[0], &mut skpv1[0], &mut e0[0], &mut e1[0], noiseseed, 0, 1, 2, 3);
    }

    #[cfg(all(feature = "kyber1024", not(feature = "90s")))]
    {
        let (skpv0, skpv1) = skpv.vec.split_at_mut(1);
        let (skpv1, skpv2) = skpv1.split_at_mut(1);
        let (skpv2, skpv3) = skpv2.split_at_mut(1);
        poly_getnoise_eta1_4x(&mut skpv0[0], &mut skpv1[0], &mut skpv2[0], &mut skpv3[0], noiseseed, 0, 1, 2, 3);
        let (e0, e1) = e.vec.split_at_mut(1);
        let (e1, e2) = e1.split_at_mut(1);
        let (e2, e3) = e2.split_at_mut(1);
        poly_getnoise_eta1_4x(&mut e0[0], &mut e1[0], &mut e2[0], &mut e3[0], noiseseed, 4, 5, 6, 7);
    }

    #[cfg(not(any(feature = "kyber1024", feature = "kyber512", feature = "90s")))] // kyber764
    {
        let (skpv0, skpv1) = skpv.vec.split_at_mut(1);
        let (skpv1, skpv2) = skpv1.split_at_mut(1);
        poly_getnoise_eta1_4x(&mut skpv0[0], &mut skpv1[0], &mut skpv2[0], &mut e.vec[0], noiseseed, 0, 1, 2, 3);
        let (e1, e2) = e.vec.split_at_mut(2);
        let (pkpv0, pkpv1) = pkpv.vec.split_at_mut(1);
        poly_getnoise_eta1_4x(&mut e1[1], &mut e2[0], &mut pkpv0[0], &mut pkpv1[0], noiseseed, 4, 5, 6, 7);
    }

    polyvec_ntt(&mut skpv);
    polyvec_reduce(&mut skpv);
    polyvec_ntt(&mut e);

    for i in 0..KYBER_K {
        polyvec_basemul_acc_montgomery(&mut pkpv.vec[i], &a[i], &skpv);
        poly_tomont(&mut pkpv.vec[i]);
    }

    polyvec_add(&mut pkpv, &e);
    polyvec_reduce(&mut pkpv);

    pack_sk(sk, &skpv);
    pack_pk(pk, &pkpv, publicseed);
}

pub fn indcpa_enc(c: &mut [u8], m: &[u8], pk: &[u8], coins: &[u8]) {
    unsafe {
        let mut at = [Polyvec::new(); KYBER_K];
        let (mut sp, mut pkpv, mut ep, mut b) = (Polyvec::new(), Polyvec::new(), Polyvec::new(), Polyvec::new());
        let (mut v, mut k, mut epp) = (Poly::new(), Poly::new(), Poly::new());
        let mut seed = [0u8; KYBER_SYMBYTES];

        unpack_pk(&mut pkpv, &mut seed, pk);
        poly_frommsg(&mut k, m);
        gen_at(&mut at, &seed);

        #[cfg(feature = "90s")]
        {
            const NOISE_NBLOCKS: usize = (KYBER_ETA1 * KYBER_N / 4) / XOF_BLOCKBYTES;
            const CIPHERTEXTNOISE_NBLOCKS: usize = (KYBER_ETA2 * KYBER_N / 4) / XOF_BLOCKBYTES;
            let mut buf = IndcpaBuf::new();
            let mut state = Aes256CtrCtx::new();
            let mut nonce = 0u64;
            aes256ctr_init(&mut state, coins, [0u8; 12]);
            nonce += 1;
            for i in 0..KYBER_K {
                aes256ctr_squeezeblocks(&mut buf.coeffs, NOISE_NBLOCKS, &mut state);
                state.n = _mm_loadl_epi64([nonce, 0].as_ptr() as *const __m128i);
                nonce += 1;
                poly_cbd_eta1_90s(&mut sp.vec[i], &buf);
            }
            for i in 0..KYBER_K {
                aes256ctr_squeezeblocks(&mut buf.coeffs, CIPHERTEXTNOISE_NBLOCKS, &mut state);
                state.n = _mm_loadl_epi64([nonce, 0].as_ptr() as *const __m128i);
                nonce += 1;
                poly_cbd_eta2(&mut ep.vec[i], &buf.vec);
            }
            aes256ctr_squeezeblocks(&mut buf.coeffs, CIPHERTEXTNOISE_NBLOCKS, &mut state);
            state.n = _mm_loadl_epi64([nonce, 0].as_ptr() as *const __m128i);
            poly_cbd_eta2(&mut epp, &buf.vec);
        }

        #[cfg(all(feature = "kyber512", not(feature = "90s")))]
        {
            let (sp0, sp1) = sp.vec.split_at_mut(1);
            let (ep0, ep1) = ep.vec.split_at_mut(1);
            poly_getnoise_eta1122_4x(&mut sp0[0], &mut sp1[0], &mut ep0[0], &mut ep1[0], coins, 0, 1, 2, 3);
            poly_getnoise_eta2(&mut epp, coins, 4);
        }

        #[cfg(not(any(feature = "kyber1024", feature = "kyber512", feature = "90s")))] // kyber764)
        {
            let (sp0, sp1) = sp.vec.split_at_mut(1);
            let (sp1, sp2) = sp1.split_at_mut(1);
            poly_getnoise_eta1_4x(&mut sp0[0], &mut sp1[0], &mut sp2[0], &mut ep.vec[0], coins, 0, 1, 2, 3);
            let (ep1, ep2) = ep.vec.split_at_mut(2);
            poly_getnoise_eta1_4x(&mut ep1[1], &mut ep2[0], &mut epp, &mut b.vec[0], coins, 4, 5, 6, 7);
        }

        #[cfg(all(feature = "kyber1024", not(feature = "90s")))]
        {
            let (sp0, sp1) = sp.vec.split_at_mut(1);
            let (sp1, sp2) = sp1.split_at_mut(1);
            let (sp2, sp3) = sp2.split_at_mut(1);
            poly_getnoise_eta1_4x(&mut sp0[0], &mut sp1[0], &mut sp2[0], &mut sp3[0], coins, 0, 1, 2, 3);
            let (ep0, ep1) = ep.vec.split_at_mut(1);
            let (ep1, ep2) = ep1.split_at_mut(1);
            let (ep2, ep3) = ep2.split_at_mut(1);
            poly_getnoise_eta1_4x(&mut ep0[0], &mut ep1[0], &mut ep2[0], &mut ep3[0], coins, 4, 5, 6, 7);
            poly_getnoise_eta2(&mut epp, coins, 8);
        }

        polyvec_ntt(&mut sp);

        for i in 0..KYBER_K {
            polyvec_basemul_acc_montgomery(&mut b.vec[i], &at[i], &sp);
        }
        polyvec_basemul_acc_montgomery(&mut v, &pkpv, &sp);

        polyvec_invntt_tomont(&mut b);
        poly_invntt_tomont(&mut v);

        polyvec_add(&mut b, &ep);
        poly_add(&mut v, &epp);
        poly_add(&mut v, &k);

        polyvec_reduce(&mut b);
        poly_reduce(&mut v);

        pack_ciphertext(c, &b, v);
    }
}

pub fn indcpa_dec(m: &mut [u8], c: &[u8], sk: &[u8]) {
    let (mut b, mut skpv) = (Polyvec::new(), Polyvec::new());
    let (mut v, mut mp) = (Poly::new(), Poly::new());

    unpack_ciphertext(&mut b, &mut v, c);
    unpack_sk(&mut skpv, sk);

    polyvec_ntt(&mut b);
    polyvec_basemul_acc_montgomery(&mut mp, &skpv, &b);

    poly_invntt_tomont(&mut mp);
    poly_sub(&mut mp, &v);
    poly_reduce(&mut mp);

    poly_tomsg(m, mp);
}
