#![cfg(not(feature = "KATs"))]
use pqc_kyber::*;

#[test]
fn keypair_encap_decap_kem() {
    let mut rng = rand::thread_rng();
    let keys = keypair(&mut rng);
    let (ct, ss1) = encapsulate(&keys.public, &mut rng).unwrap();
    let ss2 = decapsulate(&ct, &keys.secret).unwrap();
    assert_eq!(ss1, ss2);
}
