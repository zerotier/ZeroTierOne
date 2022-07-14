#![allow(clippy::needless_range_loop, dead_code)]

use crate::symmetric::KeccakState;

pub(crate) const SHAKE128_RATE: usize = 168;
const SHAKE256_RATE: usize = 136;
const SHA3_256_RATE: usize = 136;
const SHA3_512_RATE: usize =  72;
const NROUNDS: usize = 24;

fn rol(a: u64, offset: u64) -> u64 
{
  (a << offset) ^ (a >> (64-offset))
}

// Name:        load64
//
// Description: Load 8 bytes into u64 in little-endian order
//
// Arguments:   - const [u8] x: input byte array
//
// Returns the loaded 64-bit unsigned integer
pub fn load64(x: &[u8]) -> u64
{
  let mut r = 0u64;
  for i in 0..8 {
    r |= (x[i] as u64) << (8 * i);
  }
  r
}

// Name:        store64
//
// Description: Store a 64-bit integer to a byte array in little-endian order
//
// Arguments:   - [u8] x: the output byte array
//              - u64 u: input 64-bit unsigned integer
pub fn store64(x: &mut[u8], mut u: u64)
{
  for i in x.iter_mut().take(8) {
    *i = u as u8;
    u >>= 8;
  }
}

// Keccak round constants
const KECCAKF_ROUNDCONSTANTS: [u64; NROUNDS] = [
  0x0000000000000001,
  0x0000000000008082,
  0x800000000000808a,
  0x8000000080008000,
  0x000000000000808b,
  0x0000000080000001,
  0x8000000080008081,
  0x8000000000008009,
  0x000000000000008a,
  0x0000000000000088,
  0x0000000080008009,
  0x000000008000000a,
  0x000000008000808b,
  0x800000000000008b,
  0x8000000000008089,
  0x8000000000008003,
  0x8000000000008002,
  0x8000000000000080,
  0x000000000000800a,
  0x800000008000000a,
  0x8000000080008081,
  0x8000000000008080,
  0x0000000080000001,
  0x8000000080008008
];

// Name:        KeccakF1600_StatePermute
//
// Description: The Keccak F1600 Permutation
//
// Arguments:   - u64 * state: in/output Keccak state
pub fn keccakf1600_statepermute(state: &mut[u64])
{
  //copyFromState(A, state)
 let mut aba = state[ 0];
 let mut abe = state[ 1];
 let mut abi = state[ 2];
 let mut abo = state[ 3];
 let mut abu = state[ 4];
 let mut aga = state[ 5];
 let mut age = state[ 6];
 let mut agi = state[ 7];
 let mut ago = state[ 8];
 let mut agu = state[ 9];
 let mut aka = state[10];
 let mut ake = state[11];
 let mut aki = state[12];
 let mut ako = state[13];
 let mut aku = state[14];
 let mut ama = state[15];
 let mut ame = state[16];
 let mut ami = state[17];
 let mut amo = state[18];
 let mut amu = state[19];
 let mut asa = state[20];
 let mut ase = state[21];
 let mut asi = state[22];
 let mut aso = state[23];
 let mut asu = state[24];

  for round in (0..NROUNDS).step_by(2) {
    // prepareTheta
    let mut bca = aba^aga^aka^ama^asa;
    let mut bce = abe^age^ake^ame^ase;
    let mut bci = abi^agi^aki^ami^asi;
    let mut bco = abo^ago^ako^amo^aso;
    let mut bcu = abu^agu^aku^amu^asu;

    //thetaRhoPiChiIotaPrepareTheta(round  , A, E)
    let mut da = bcu^rol(bce, 1);
    let mut de = bca^rol(bci, 1);
    let mut di = bce^rol(bco, 1);
    let mut d_o = bci^rol(bcu, 1);
    let mut du = bco^rol(bca, 1);

    aba ^= da;
    bca = aba;
    age ^= de;
    bce = rol(age, 44);
    aki ^= di;
    bci = rol(aki, 43);
    amo ^= d_o;
    bco = rol(amo, 21);
    asu ^= du;
    bcu = rol(asu, 14);
    let mut eba =   bca ^((!bce)&  bci );
    eba ^= KECCAKF_ROUNDCONSTANTS[round];
    let mut ebe =   bce ^((!bci)&  bco );
    let mut ebi =   bci ^((!bco)&  bcu );
    let mut ebo =   bco ^((!bcu)&  bca );
    let mut ebu =   bcu ^((!bca)&  bce );

    abo ^= d_o;
    bca = rol(abo, 28);
    agu ^= du;
    bce = rol(agu, 20);
    aka ^= da;
    bci = rol(aka,  3);
    ame ^= de;
    bco = rol(ame, 45);
    asi ^= di;
    bcu = rol(asi, 61);
    let mut ega =   bca ^((!bce)&  bci );
    let mut ege =   bce ^((!bci)&  bco );
    let mut egi =   bci ^((!bco)&  bcu );
    let mut ego =   bco ^((!bcu)&  bca );
    let mut egu =   bcu ^((!bca)&  bce );

    abe ^= de;
    bca = rol(abe,  1);
    agi ^= di;
    bce = rol(agi,  6);
    ako ^= d_o;
    bci = rol(ako, 25);
    amu ^= du;
    bco = rol(amu,  8);
    asa ^= da;
    bcu = rol(asa, 18);
    let mut eka =   bca ^((!bce)&  bci );
    let mut eke =   bce ^((!bci)&  bco );
    let mut eki =   bci ^((!bco)&  bcu );
    let mut eko =   bco ^((!bcu)&  bca );
    let mut eku =   bcu ^((!bca)&  bce );

    abu ^= du;
    bca = rol(abu, 27);
    aga ^= da;
    bce = rol(aga, 36);
    ake ^= de;
    bci = rol(ake, 10);
    ami ^= di;
    bco = rol(ami, 15);
    aso ^= d_o;
    bcu = rol(aso, 56);
    let mut ema =   bca ^((!bce)&  bci );
    let mut eme =   bce ^((!bci)&  bco );
    let mut emi =   bci ^((!bco)&  bcu );
    let mut emo =   bco ^((!bcu)&  bca );
    let mut emu =   bcu ^((!bca)&  bce );

    abi ^= di;
    bca = rol(abi, 62);
    ago ^= d_o;
    bce = rol(ago, 55);
    aku ^= du;
    bci = rol(aku, 39);
    ama ^= da;
    bco = rol(ama, 41);
    ase ^= de;
    bcu = rol(ase,  2);
    let mut esa =   bca ^((!bce)&  bci );
    let mut ese =   bce ^((!bci)&  bco );
    let mut esi =   bci ^((!bco)&  bcu );
    let mut eso =   bco ^((!bcu)&  bca );
    let mut esu =   bcu ^((!bca)&  bce );

    //    prepareTheta
    bca = eba^ega^eka^ema^esa;
    bce = ebe^ege^eke^eme^ese;
    bci = ebi^egi^eki^emi^esi;
    bco = ebo^ego^eko^emo^eso;
    bcu = ebu^egu^eku^emu^esu;

    //thetaRhoPiChiIotaPrepareTheta(round+1, E, A)
    da = bcu^rol(bce, 1);
    de = bca^rol(bci, 1);
    di = bce^rol(bco, 1);
    d_o = bci^rol(bcu, 1);
    du = bco^rol(bca, 1);

    eba ^= da;
    bca = eba;
    ege ^= de;
    bce = rol(ege, 44);
    eki ^= di;
    bci = rol(eki, 43);
    emo ^= d_o;
    bco = rol(emo, 21);
    esu ^= du;
    bcu = rol(esu, 14);
    aba =   bca ^((!bce)&  bci );
    aba ^= KECCAKF_ROUNDCONSTANTS[round+1];
    abe =   bce ^((!bci)&  bco );
    abi =   bci ^((!bco)&  bcu );
    abo =   bco ^((!bcu)&  bca );
    abu =   bcu ^((!bca)&  bce );

    ebo ^= d_o;
    bca = rol(ebo, 28);
    egu ^= du;
    bce = rol(egu, 20);
    eka ^= da;
    bci = rol(eka, 3);
    eme ^= de;
    bco = rol(eme, 45);
    esi ^= di;
    bcu = rol(esi, 61);
    aga =   bca ^((!bce)&  bci );
    age =   bce ^((!bci)&  bco );
    agi =   bci ^((!bco)&  bcu );
    ago =   bco ^((!bcu)&  bca );
    agu =   bcu ^((!bca)&  bce );

    ebe ^= de;
    bca = rol(ebe, 1);
    egi ^= di;
    bce = rol(egi, 6);
    eko ^= d_o;
    bci = rol(eko, 25);
    emu ^= du;
    bco = rol(emu, 8);
    esa ^= da;
    bcu = rol(esa, 18);
    aka =   bca ^((!bce)&  bci );
    ake =   bce ^((!bci)&  bco );
    aki =   bci ^((!bco)&  bcu );
    ako =   bco ^((!bcu)&  bca );
    aku =   bcu ^((!bca)&  bce );

    ebu ^= du;
    bca = rol(ebu, 27);
    ega ^= da;
    bce = rol(ega, 36);
    eke ^= de;
    bci = rol(eke, 10);
    emi ^= di;
    bco = rol(emi, 15);
    eso ^= d_o;
    bcu = rol(eso, 56);
    ama =   bca ^((!bce)&  bci );
    ame =   bce ^((!bci)&  bco );
    ami =   bci ^((!bco)&  bcu );
    amo =   bco ^((!bcu)&  bca );
    amu =   bcu ^((!bca)&  bce );

    ebi ^= di;
    bca = rol(ebi, 62);
    ego ^= d_o;
    bce = rol(ego, 55);
    eku ^= du;
    bci = rol(eku, 39);
    ema ^= da;
    bco = rol(ema, 41);
    ese ^= de;
    bcu = rol(ese, 2);
    asa =   bca ^((!bce)&  bci );
    ase =   bce ^((!bci)&  bco );
    asi =   bci ^((!bco)&  bcu );
    aso =   bco ^((!bcu)&  bca );
    asu =   bcu ^((!bca)&  bce );
  } 

  state[ 0] = aba;
  state[ 1] = abe;
  state[ 2] = abi;
  state[ 3] = abo;
  state[ 4] = abu;
  state[ 5] = aga;
  state[ 6] = age;
  state[ 7] = agi;
  state[ 8] = ago;
  state[ 9] = agu;
  state[10] = aka;
  state[11] = ake;
  state[12] = aki;
  state[13] = ako;
  state[14] = aku;
  state[15] = ama;
  state[16] = ame;
  state[17] = ami;
  state[18] = amo;
  state[19] = amu;
  state[20] = asa;
  state[21] = ase;
  state[22] = asi;
  state[23] = aso;
  state[24] = asu;
}

// Name:        keccak_absorb
//
// Description: Absorb step of Keccak;
//              non-incremental, starts by zeroeing the state.
//
// Arguments:   - u64 s:             Keccak state
//                usize pos: position in current block to be absorbed
//              - usize r:          rate in bytes (e.g., 168 for SHAKE128)
//              - const [u8] input: pointer to input to be absorbed into s
//              - u64 inlen: length of input in bytes
pub(crate) fn keccak_absorb(
  s: &mut[u64], 
  mut pos: usize, 
  r: usize, 
  input: &[u8], 
  mut inlen: usize
) -> usize
{
  let mut idx = 0usize;
  while pos+inlen >= r {
    for i in pos..r {
      s[i/8] ^= (input[idx] as u64) << 8 * (i%8);
      idx += 1;
    }
    inlen -= r-pos;
    keccakf1600_statepermute(s);
    pos = 0;
  }
  let new_pos = pos+inlen;
  for i in pos..new_pos {
    s[i/8] ^= (input[idx] as u64) << 8 * (i%8);
  }
  new_pos
}

// Name:        keccak_squeezeblocks
//
// Description: Squeeze step of Keccak. Squeezes full blocks of r bytes each.
//              Modifies the state. Can be called multiple times to keep squeezing,
//              i.e., is incremental.
//
// Arguments:   - [u8] h:             output blocks
//              - u64 nblocks:        number of blocks to be squeezed (written to h)
//              - u64 *s:             in/output Keccak state
//              - usize r:            rate in bytes (e.g., 168 for SHAKE128)
pub(crate) fn keccak_squeezeblocks(h: &mut[u8], mut nblocks: usize, s: &mut [u64], r: usize)
{
  let mut idx = 0usize;
  while nblocks > 0 {
    keccakf1600_statepermute(s);
    for i in 0..r/8 {
      store64(&mut h[idx+8*i..], s[i])
    }
    idx += r;
    nblocks -= 1;
  }
}

// Name:        shake128_absorb
//
// Description: Absorb step of the SHAKE128 XOF.
//              non-incremental, starts by zeroeing the state.
//
// Arguments:   - u64 *s:                     (uninitialized) output Keccak state
//              - const [u8] input:      input to be absorbed into s
//              - u64 inputByteLen: length of input in bytes
pub(crate) fn shake128_absorb(state: &mut KeccakState, input: &[u8], inlen: usize)
{
  let pos = state.pos;
  state.pos =keccak_absorb(&mut state.s, pos, SHAKE128_RATE, input, inlen);
}

// Name:        shake128_squeezeblocks
//
// Description: Squeeze step of SHAKE128 XOF. Squeezes full blocks of
//              SHAKE128_RATE bytes each. Can be called multiple times
//              to keep squeezing. Assumes new block has not yet been
//              started (state->pos = SHAKE128_RATE).
//
// Arguments:   - [u8] out: pointer to output blocks
//              - u64 nblocks: number of blocks to be squeezed (written to output)
//              - KeccakState state: pointer to input/output Keccak state
pub(crate) fn shake128_squeezeblocks(out: &mut[u8], nblocks: usize, state: &mut KeccakState)
{
  keccak_squeezeblocks(out, nblocks, &mut state.s, SHAKE128_RATE);
}

// Name:        shake256
//
// Description: SHAKE256 XOF with non-incremental API
//
// Arguments:   - [u8] output:      output
//              - usize outlen:  requested output length in bytes
//              - [u8] input: input
//              - usize inlen:   length of input in bytes
pub(crate) fn shake256(out: &mut[u8], mut outlen: usize, input: &[u8], inlen: usize)
{
  let mut state = KeccakState::new();
  let mut idx = 0;
  shake256_absorb_once(&mut state, input, inlen);
  let nblocks = outlen/SHAKE256_RATE;
  shake256_squeezeblocks(&mut out[idx..], nblocks, &mut state);
  outlen -= nblocks*SHAKE256_RATE;
  idx += nblocks*SHAKE256_RATE;
  shake256_squeeze(&mut out[idx..], outlen, &mut state);
}

// Name:        sha3_256
//
// Description: SHA3-256 with non-incremental API
//
// Arguments:   - [u8] h:      output (32 bytes)
//              - const [u8] input: input
//              - usize inlen:   length of input in bytes
pub(crate) fn sha3_256(h: &mut[u8], input: &[u8], inlen: usize)
{
  let mut s = [0u64; 25]; 
  keccak_absorb_once(&mut s, SHA3_256_RATE, input, inlen, 0x06);
  keccakf1600_statepermute(&mut s);
  for i in 0..4 {
    store64(&mut h[8*i..], s[i]);
  }
}

// Name:        sha3_512
//
// Description: SHA3-512 with non-incremental API
//
// Arguments:   - [u8] h:      output (64 bytes)
//              - const [u8] input: input
//              - usize inlen:   length of input in bytes
pub(crate) fn sha3_512(h: &mut[u8], input: &[u8], inlen: usize)
{
  let mut s = [0u64; 25]; 
  keccak_absorb_once(&mut s, SHA3_512_RATE, input, inlen, 0x06);
  keccakf1600_statepermute(&mut s);
  for i in 0..8 {
    store64(&mut h[8*i..], s[i]);
  }
}



// Name:        keccak_finalize
//
// Description: Finalize absorb step.
//
// Arguments:   - u64 s: pointer to Keccak state
//              - usize pos: position in current block to be absorbed
//              - usize r: rate in bytes (e.g., 168 for SHAKE128)
//              - u8 p: domain separation byte
fn keccak_finalize(s: &mut[u64], pos: usize, r: usize, p: u8)
{
  s[pos/8] ^= (p as u64) << 8*(pos%8);
  s[r/8-1] ^= 1u64 << 63;
}

// Name:        keccak_absorb_once
//
// Description: Absorb step of Keccak;
//              non-incremental, starts by zeroeing the state.
//
// Arguments:   - u64 *s:             (uninitialized) output Keccak state
//              - usize r:          rate in bytes (e.g., 168 for SHAKE128)
//              - const [u8] input:  input to be absorbed into s
//              - u64 mlen: length of input in bytes
//              - [u8]  p:         domain-separation byte for different Keccak-derived functions
pub(crate) fn keccak_absorb_once(
  s: &mut[u64], 
  r: usize, 
  input: &[u8], 
  mut inlen: 
  usize, 
  p: u8)
{
  // Zero State
  for i in s.iter_mut() {
    *i = 0;
  }

  let mut idx = 0usize;
  while inlen >= r {
    for i in 0..(r/8) {
      s[i] ^= load64(&input[idx+8*i..]);
    }
    idx += r;
    inlen -= r;
    keccakf1600_statepermute(s);
  }

  for i in 0..inlen {
    s[i/8] ^= (input[idx+i] as u64) << 8*(i%8);
  }
  s[inlen/8] ^= (p as u64) << 8*(inlen%8);
  s[(r-1)/8] ^= 1u64 << 63;
}

// Name:        keccak_squeeze
//
// Description: Squeeze step of Keccak. Squeezes full blocks of r bytes each.
//              Modifies the state. Can be called multiple times to keep squeezing,
//              i.e., is incremental.
//
// Arguments:   - [u8] out:             output blocks
//              - u64 nblocks:        number of blocks to be squeezed (written to out)
//              - u64 *s:             in/output Keccak state
//                usize pos: number of bytes in current block already squeezed
//              - usize r:            rate in bytes (e.g., 168 for SHAKE128)
// Returns new position pos in current block
pub(crate) fn keccak_squeeze(
  out: &mut[u8], 
  mut outlen: usize, 
  s: &mut [u64], 
  mut pos: usize, 
  r: usize
) -> usize
{
  let mut idx = 0;
  while outlen > 0 {
    if pos == r {
      keccakf1600_statepermute(s);
      pos = 0
    }
    let mut i = pos;
    while i < r  && i < pos+outlen {
      out[idx] = (s[i/8] >> 8*(i%8)) as u8;
      i += 1;
      idx += 1;
    }
    outlen -= i-pos;
    pos = i;
  }
  pos
}

// Name:        shake128_init
//
// Description: Initilizes Keccak state for use as SHAKE128 XOF
//
// Arguments:   - keccak_state state: (uninitialized) Keccak state
fn shake128_init(state: &mut KeccakState)
{
  state.reset()
}


// Name:        shake128_finalize
//
// Description: Finalize absorb step of the SHAKE128 XOF.
//
// Arguments:   - keccak_state state: pointer to Keccak state
fn shake128_finalize(state: &mut KeccakState)
{
  keccak_finalize(&mut state.s, state.pos, SHAKE128_RATE, 0x1F);
  state.pos = SHAKE128_RATE;
}

// Name:        shake128_squeeze
//
// Description: Squeeze step of SHAKE128 XOF. Squeezes arbitraily many
//              bytes. Can be called multiple times to keep squeezing.
//
// Arguments:   - [u8] out: pointer to output blocks
//              - usize outlen : number of bytes to be squeezed (written to output)
//              - keccak_state s: pointer to input/output Keccak state
fn shake128_squeeze(out: &mut[u8], outlen: usize, state: &mut KeccakState)
{
  state.pos = keccak_squeeze(out, outlen, &mut state.s, state.pos, SHAKE128_RATE);
}

// Name:        shake128_absorb_once
//
// Description: Initialize, absorb into and finalize SHAKE128 XOF; non-incremental.
//
// Arguments:   - keccak_state state: pointer to (uninitialized) output Keccak state
//              - const [u8] in: input to be absorbed into s
//              - usize inlen: length of input in bytes
pub(crate) fn shake128_absorb_once(state: &mut KeccakState, input: &[u8], inlen: usize)
{
  keccak_absorb_once(&mut state.s, SHAKE128_RATE, input, inlen, 0x1F);
  state.pos = SHAKE128_RATE;
}

fn shake256_init(state: &mut KeccakState) {
  state.reset();
}

fn shake256_absorb(state: &mut KeccakState,  input: &[u8], inlen: usize)
{
  state.pos = keccak_absorb(&mut state.s, state.pos, SHAKE256_RATE, input, inlen);
}

fn shake256_finalize(state: &mut KeccakState)
{
  keccak_finalize(&mut state.s, state.pos, SHAKE256_RATE, 0x1F);
  state.pos = SHAKE256_RATE;
}

fn shake256_squeeze(out: &mut[u8], outlen: usize, state: &mut KeccakState)
{
  state.pos = keccak_squeeze(out, outlen, &mut state.s, state.pos, SHAKE256_RATE);
}

fn shake256_absorb_once(state: &mut KeccakState, input: &[u8], inlen: usize)
{
  keccak_absorb_once(&mut state.s, SHAKE256_RATE, input, inlen, 0x1F);
  state.pos = SHAKE256_RATE;
}

fn shake256_squeezeblocks(out: &mut[u8], nblocks: usize, state: &mut KeccakState)
{
  keccak_squeezeblocks(out, nblocks, &mut state.s, SHAKE256_RATE);
}

fn shake128(out: &mut[u8], mut outlen: usize, input: &[u8], inlen: usize)
{
  let mut state = KeccakState::new();
  let mut idx = 0;
  shake128_absorb_once(&mut state, input, inlen);
  let nblocks = outlen/SHAKE128_RATE;
  shake128_squeezeblocks(&mut out[idx..], nblocks, &mut state);
  outlen -= nblocks*SHAKE128_RATE;
  idx += nblocks*SHAKE128_RATE;
  shake128_squeeze(&mut out[idx..], outlen, &mut state);
}

