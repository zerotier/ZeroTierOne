use pqc_kyber::*;

fn main() -> Result<(), KyberError> {
    let mut rng = rand::thread_rng();

    let mut alice = Ake::new();
    let mut bob = Ake::new();
    let alice_keys = keypair(&mut rng);
    let bob_keys = keypair(&mut rng);

    // Alice initiates key exchange with bob
    let client_send = alice.client_init(&bob_keys.public, &mut rng);

    // Bob receives the request and authenticates Alice, sends
    // encapsulated shared secret back
    let server_send = bob.server_receive(client_send, &alice_keys.public, &bob_keys.secret, &mut rng)?;

    // Alice autheticates and decapsulates
    alice.client_confirm(server_send, &alice_keys.secret)?;

    // Both structs now have the shared secret
    assert_eq!(alice.shared_secret, bob.shared_secret);

    Ok(())
}
