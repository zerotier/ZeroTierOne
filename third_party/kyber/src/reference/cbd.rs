use crate::poly::Poly;
use crate::params::KYBER_N;

// Name:        load32_littleendian
//
// Description: load 4 bytes into a 32-bit integer
//              in little-endian order
//
// Arguments:   - const [u8] x: input byte array
//
// Returns 32-bit unsigned integer loaded from x
fn load32_littleendian(x: &[u8]) -> u32 
{
  let mut r = x[0] as u32;
  r |= (x[1] as u32) << 8;
  r |= (x[2] as u32) << 16;
  r |= (x[3] as u32) << 24;
  r
}

// Name:        load32_littleendian
//
// Description: load 3 bytes into a 32-bit integer
//              in little-endian order
//              This function is only needed for Kyber-512
//
// Arguments:   - const [u8] x: input byte array
//
// Returns 32-bit unsigned integer loaded from x
fn load24_littleendian(x: &[u8]) -> u32 
{
  let mut r = x[0] as u32;
  r |= (x[1] as u32) << 8;
  r |= (x[2] as u32) << 16;
  r
}

// Name:        cbd2
//
// Description: Given an array of uniformly random bytes, compute
//              polynomial with coefficients distributed according to
//              a centered binomial distribution with parameter eta=2
//
// Arguments:   - poly *r:                  output polynomial
//              - const [u8] buf: input byte array
pub fn cbd2(r: &mut Poly, buf: &[u8])
{
  let (mut d, mut t, mut a, mut b); 
  for i in 0..(KYBER_N/8) {
    t = load32_littleendian(&buf[4*i..]);
    d = t & 0x55555555;
    d += (t>>1) & 0x55555555;
    for j in 0..8 {
      a = ((d >>  (4*j))  & 0x3) as i16;
      b = ((d >> (4*j+2)) & 0x3) as i16;
      r.coeffs[8*i+j] = a - b;
    }
  }
}

// Name:        cbd3
//
// Description: Given an array of uniformly random bytes, compute
//              polynomial with coefficients distributed according to
//              a centered binomial distribution with parameter eta=3
//              This function is only needed for Kyber-512
// Arguments:   - poly *r:                  output polynomial
//              - const [u8] buf: input byte array
pub fn cbd3(r: &mut Poly, buf: &[u8])
{
  let (mut d, mut t, mut a, mut b); 
  for i in 0..(KYBER_N/4) {
    t = load24_littleendian(&buf[3*i..]);
    d  = t & 0x00249249;
    d += (t>>1) & 0x00249249;
    d += (t>>2) & 0x00249249;
    for j in 0..4 {
      a = ((d >>  (6*j))  & 0x7) as i16;
      b = ((d >> (6*j+3)) & 0x7) as i16;
      r.coeffs[4*i+j] = a - b;
    }
  }
}

pub fn poly_cbd_eta1(r: &mut Poly, buf: &[u8]) 
{
  if cfg!(feature="kyber512") {
    cbd3(r, buf)
  } 
  else {
    cbd2(r, buf)
  }
}

pub fn poly_cbd_eta2(r: &mut Poly, buf: &[u8]) 
{
  cbd2(r, buf)
}
