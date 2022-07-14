use pqc_kyber::*;

fn main () -> Result<(), KyberError> {
  let mut rng = rand::thread_rng();

  // Alice generates a keypair
  let alice_keys = keypair(&mut rng);

  // Bob encapsulates a shared secret
  let (ciphertext, shared_secret_bob) = encapsulate(&alice_keys.public, &mut rng)?;
  
  // Alice decapsulates the shared secret
  let shared_secret_alice = decapsulate(&ciphertext, &alice_keys.secret)?;
  
  // Both can now communicate symetrically
  assert_eq!(shared_secret_alice, shared_secret_bob);
  Ok(())
}