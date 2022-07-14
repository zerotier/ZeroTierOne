use honggfuzz::fuzz;
use pqc_kyber::*;
use rand_xoshiro::rand_core::{SeedableRng, RngCore};
use rand_xoshiro::Xoshiro256Plus;

fn main() -> Result<(), KyberError> {
  let mut _rng = rand::thread_rng(); //placeholder
  let mut rng = Xoshiro256Plus::seed_from_u64(0);
  let mut ct = [0u8; KYBER_CIPHERTEXTBYTES];
  let mut ss = [0u8; KYBER_SSBYTES];
  let mut s1 = [0u8; 32];
  loop {
    rng.fill_bytes(&mut s1);
    fuzz!(|data: &[u8] | {
      if data.len() != KYBER_PUBLICKEYBYTES {return};
      crypto_kem_enc(&mut ct, &mut ss, data, &mut _rng, Some(&s1));
    });
  };
}