use honggfuzz::fuzz;
use pqc_kyber::*;

fn main() -> Result<(), KyberError> {
  let mut ss = [0u8; KYBER_SSBYTES];
  const CTBYTES: usize = KYBER_CIPHERTEXTBYTES;
  const SKBYTES: usize = KYBER_SECRETKEYBYTES;
  loop {
    fuzz!(|data: &[u8] | {
      if data.len() != CTBYTES + SKBYTES  {return};
      match crypto_kem_dec(&mut ss, &data[..CTBYTES], &data[CTBYTES..SKBYTES]) {
        Ok(_) => (),
        Err(_) => ()
      }
    });
  };
}