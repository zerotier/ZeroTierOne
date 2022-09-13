use honggfuzz::fuzz;
use pqc_kyber::*;
use rand_xoshiro::rand_core::{RngCore, SeedableRng};
use rand_xoshiro::Xoshiro256Plus;

fn main() -> Result<(), KyberError> {
    let mut _rng = rand::thread_rng(); //placeholder
    let mut rng = Xoshiro256Plus::seed_from_u64(0);
    let mut public = [0u8; KYBER_PUBLICKEYBYTES];
    let mut secret = [0u8; KYBER_SECRETKEYBYTES];
    let mut s1 = [0u8; 32];
    let mut s2 = [0u8; 32];
    loop {
        rng.fill_bytes(&mut s1);
        rng.fill_bytes(&mut s2);
        fuzz!(|data: ()| {
            crypto_kem_keypair(&mut public, &mut secret, &mut _rng, Some((&s1, &s2)));
        });
    }
}
