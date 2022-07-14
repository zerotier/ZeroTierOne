#[cfg(not(feature="KATs"))]
use crate::rng::randombytes;
use crate::{
  poly::*,
  polyvec::*,
  symmetric::*,
  params::*,
  RngCore,
  CryptoRng,
};

// Name:        pack_pk
//
// Description: Serialize the public key as concatenation of the
//              serialized vector of polynomials pk
//              and the public seed used to generate the matrix A.
//
// Arguments:   [u8] r:          the output serialized public key
//              const poly *pk:            the input public-key polynomial
//              const [u8] seed: the input public seed
fn pack_pk(r: &mut[u8], pk: &mut Polyvec, seed: &[u8])
{
  const END: usize = KYBER_SYMBYTES + KYBER_POLYVECBYTES;
  polyvec_tobytes(r, pk);
  r[KYBER_POLYVECBYTES..END].copy_from_slice(&seed[..KYBER_SYMBYTES]);
}

// Name:        unpack_pk
//
// Description: De-serialize public key from a byte array;
//              approximate inverse of pack_pk
//
// Arguments:   - Polyvec pk:          output public-key vector of polynomials
//              - [u8] seed:           output seed to generate matrix A
//              - const [u8] packedpk: input serialized public key
fn unpack_pk(pk: &mut Polyvec, seed: &mut[u8], packedpk: &[u8])
{
  const END: usize = KYBER_SYMBYTES + KYBER_POLYVECBYTES;
  polyvec_frombytes(pk, packedpk);
  seed[..KYBER_SYMBYTES].copy_from_slice(&packedpk[KYBER_POLYVECBYTES..END]);
}

// Name:        pack_sk
//
// Description: Serialize the secret key
//
// Arguments: - [u8] r:  output serialized secret key
//            - const Polyvec sk: input vector of polynomials (secret key)
fn pack_sk(r: &mut[u8], sk: &mut Polyvec)
{
  polyvec_tobytes(r, sk);
}

// Name:        unpack_sk
//
// Description: De-serialize the secret key, inverse of pack_sk
//
// Arguments:   - Polyvec sk: output vector of polynomials (secret key)
//              - const [u8] packedsk: input serialized secret key
fn unpack_sk(sk: &mut Polyvec, packedsk: &[u8])
{
  polyvec_frombytes(sk, packedsk);
}

// Name:        pack_ciphertext
//
// Description: Serialize the ciphertext as concatenation of the
//              compressed and serialized vector of polynomials b
//              and the compressed and serialized polynomial v
//
// Arguments:   [u8] r:          the output serialized ciphertext
//              const poly *pk:  the input vector of polynomials b
//              const [u8] seed: the input polynomial v
fn pack_ciphertext(r: &mut[u8], b: &mut Polyvec, v: Poly)
{
  polyvec_compress(r, *b);
  poly_compress(&mut r[KYBER_POLYVECCOMPRESSEDBYTES..], v);
}

// Name:        unpack_ciphertext
//
// Description: De-serialize and decompress ciphertext from a byte array;
//              approximate inverse of pack_ciphertext
//
// Arguments:   - Polyvec b:             output vector of polynomials b
//              - poly *v:                output polynomial v
//              - const [u8] c:           input serialized ciphertext
fn unpack_ciphertext(b: &mut Polyvec, v: &mut Poly, c: &[u8])
{
  polyvec_decompress(b, c);
  poly_decompress(v, &c[KYBER_POLYVECCOMPRESSEDBYTES..]);
}

// Name:        rej_uniform
//
// Description: Run rejection sampling on uniform random bytes to generate
//              uniform random integers mod q
//
// Arguments: - i16 *r:        output buffer
//            - usize len:         requested number of 16-bit integers (uniform mod q)
//            - const [u8] buf:    input buffer (assumed to be uniform random bytes)
//            - usize buflen:      length of input buffer in bytes
//
// Returns number of sampled 16-bit integers (at most len)
fn rej_uniform(r: &mut[i16], len: usize, buf: &[u8], buflen: usize) -> usize
{
  let (mut ctr, mut pos) = (0usize, 0usize);
  let (mut val0, mut val1);

  while ctr < len && pos + 3 <= buflen {
    val0 = ((buf[pos+0] >> 0) as u16 | (buf[pos+1] as u16) << 8) & 0xFFF;
    val1 = ((buf[pos+1] >> 4) as u16 | (buf[pos+2] as u16) << 4) & 0xFFF;
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

fn gen_a(a: &mut [Polyvec], b: &[u8]) 
{
  gen_matrix(a, b, false);
}

fn gen_at(a: &mut [Polyvec], b: &[u8]) 
{
  gen_matrix(a, b, true);
}

// Name:        gen_matrix
//
// Description: Deterministically generate matrix A (or the transpose of A)
//              from a seed. Entries of the matrix are polynomials that look
//              uniformly random. Performs rejection sampling on output of
//              a XOF
//
// Arguments:   - Polyvec a:       ouptput matrix A
//              - const [u8] seed: input seed
//              - bool transposed: boolean deciding whether A or A^T is generated
fn gen_matrix(a: &mut [Polyvec], seed: &[u8], transposed: bool)
{ 
  let mut ctr;
  // 530 is expected number of required bytes
  const GEN_MATRIX_NBLOCKS: usize = 
    (12*KYBER_N/8*(1 << 12)/KYBER_Q + XOF_BLOCKBYTES)/XOF_BLOCKBYTES;
  const BUFLEN: usize = GEN_MATRIX_NBLOCKS*XOF_BLOCKBYTES;
  let mut buf = [0u8; BUFLEN+2];
  let mut off: usize;
  let mut state = XofState::new();

  for i in 0..KYBER_K {
    for j in 0..KYBER_K {
      if transposed {
        xof_absorb(&mut state, seed, i as u8, j as u8);
      }
      else {
        xof_absorb(&mut state, seed, j as u8, i as u8);
      }
      xof_squeezeblocks(&mut buf, GEN_MATRIX_NBLOCKS, &mut state);
      ctr = rej_uniform(&mut a[i].vec[j].coeffs, KYBER_N, &buf, BUFLEN);

      while ctr < KYBER_N
      {
        off = BUFLEN % 3;
        for k in 0..off {
          buf[k] = buf[BUFLEN - off + k];
        }
        xof_squeezeblocks(&mut buf[off..], 1, &mut state);
        ctr += rej_uniform(&mut a[i].vec[j].coeffs[ctr..], KYBER_N - ctr, &buf, BUFLEN);
      }
    }
  }
}

// Name:        indcpa_keypair
//
// Description: Generates public and private key for the CPA-secure
//              public-key encryption scheme underlying Kyber
//
// Arguments: - [u8] pk: output public key (length KYBER_INDCPA_PUBLICKEYBYTES)
//            - [u8] sk: output private key (length KYBER_INDCPA_SECRETKEYBYTES)
pub fn indcpa_keypair<R>(
  pk : &mut[u8], 
  sk: &mut[u8], 
  _seed: Option<(&[u8], &[u8])>, 
  _rng: &mut R
)
  where R: CryptoRng + RngCore
{
  let mut a = [Polyvec::new(); KYBER_K];
  let (mut e, mut pkpv, mut skpv) = (Polyvec::new(), Polyvec::new(), Polyvec::new());
  let mut nonce = 0u8;
  let mut buf = [0u8; 2*KYBER_SYMBYTES];
  let mut randbuf = [0u8; 2*KYBER_SYMBYTES];

  #[cfg(not(feature="KATs"))]
  randombytes(&mut randbuf, KYBER_SYMBYTES, _rng);
  
  // Use rng seed for test vectors
  #[cfg(feature="KATs")]
  randbuf[..KYBER_SYMBYTES].copy_from_slice(&_seed.expect("KAT feature only for testing").0);
  
  hash_g(&mut buf, &randbuf, KYBER_SYMBYTES);

  let (publicseed, noiseseed) = buf.split_at(KYBER_SYMBYTES);
  gen_a(&mut a, publicseed);

  for i in 0..KYBER_K {
    poly_getnoise_eta1(&mut skpv.vec[i], noiseseed, nonce);
    nonce += 1;
  }
  for i in 0..KYBER_K {
    poly_getnoise_eta1(&mut e.vec[i], noiseseed, nonce);
    nonce += 1;
  }
  
  polyvec_ntt(&mut skpv);
  polyvec_ntt(&mut e);

  // matrix-vector multiplication
  for i in 0..KYBER_K {
    polyvec_basemul_acc_montgomery(&mut pkpv.vec[i], &a[i], &skpv);
    poly_frommont(&mut pkpv.vec[i]);
  }
  polyvec_add(&mut pkpv, &e);
  polyvec_reduce(&mut pkpv);

  pack_sk(sk, &mut skpv);
  pack_pk(pk, &mut pkpv, publicseed);
}

// Name:        indcpa_enc
//
// Description: Encryption function of the CPA-secure
//              public-key encryption scheme underlying Kyber.
//
// Arguments: - [u8] c:          output ciphertext (length KYBER_INDCPA_BYTES)
//            - const [u8] m:    input message (length KYBER_INDCPA_MSGBYTES)
//            - const [u8] pk:   input public key (length KYBER_INDCPA_PUBLICKEYBYTES)
//            - const [u8] coin: input random coins used as seed (length KYBER_SYMBYTES)
//                                  to deterministically generate all randomness
pub fn indcpa_enc(c: &mut[u8], m: &[u8], pk: &[u8], coins: &[u8])
{
  let mut at = [Polyvec::new(); KYBER_K];
  let (mut sp, mut pkpv, mut ep, mut b) = 
    (Polyvec::new(),Polyvec::new(), Polyvec::new(), Polyvec::new());
  let (mut v, mut k, mut epp) = (Poly::new(), Poly::new(), Poly::new());
  let mut seed = [0u8; KYBER_SYMBYTES];
  let mut nonce = 0u8;
  
  unpack_pk(&mut pkpv, &mut seed, pk);
  poly_frommsg(&mut k, m);
  gen_at(&mut at, &seed);

  for i in 0..KYBER_K {
    poly_getnoise_eta1(&mut sp.vec[i], coins, nonce);
    nonce += 1;
  }
  for i in 0..KYBER_K {
    poly_getnoise_eta2(&mut ep.vec[i], coins, nonce);
    nonce += 1;
  }
  poly_getnoise_eta2(&mut epp, coins, nonce);

  polyvec_ntt(&mut sp);

  // matrix-vector multiplication
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

  pack_ciphertext(c, &mut b, v);
}

// Name:        indcpa_dec
//
// Description: Decryption function of the CPA-secure
//              public-key encryption scheme underlying Kyber.
//
// Arguments:   - [u8] m:        output decrypted message (of length KYBER_INDCPA_MSGBYTES)
//              - const [u8] c:  input ciphertext (of length KYBER_INDCPA_BYTES)
//              - const [u8] sk: input secret key (of length KYBER_INDCPA_SECRETKEYBYTES)
pub fn indcpa_dec(m: &mut[u8], c: &[u8], sk: &[u8])
{
  let (mut b, mut skpv) = (Polyvec::new(),Polyvec::new());
  let (mut v, mut mp) = (Poly::new(),Poly::new());
 
  unpack_ciphertext(&mut b, &mut v, c);
  unpack_sk(&mut skpv, sk);

  polyvec_ntt(&mut b);
  polyvec_basemul_acc_montgomery(&mut mp, &skpv, &b);
  poly_invntt_tomont(&mut mp);

  poly_sub(&mut mp, &v);
  poly_reduce(&mut mp);

  poly_tomsg(m, mp);
}