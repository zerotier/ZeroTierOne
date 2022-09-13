use pqc_kyber::*;

fn main() -> Result<(), KyberError> {
    let mut rng = rand::thread_rng();

    let mut alice = Uake::new();
    let mut bob = Uake::new();

    let bob_keys = keypair(&mut rng);

    // Alice initiates key exchange with bob
    let client_send = alice.client_init(&bob_keys.public, &mut rng);

    // Bob receives the request and authenticates Alice, sends
    // encapsulated shared secret back
    let server_send = bob.server_receive(client_send, &bob_keys.secret, &mut rng)?;

    // Alice autheticates and decapsulates
    alice.client_confirm(server_send)?;

    // Both structs now have the shared secret
    assert_eq!(alice.shared_secret, bob.shared_secret);

    Ok(())
}
