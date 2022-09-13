#![cfg(not(feature = "KATs"))]
use pqc_kyber::*;

// Kyber struct uake and ake functions
#[test]
fn kyber_uake() {
    let mut rng = rand::thread_rng();
    let mut alice = Uake::new();
    let mut bob = Uake::new();
    let bob_keys = keypair(&mut rng);
    let client_init = alice.client_init(&bob_keys.public, &mut rng);
    let server_send = bob.server_receive(client_init, &bob_keys.secret, &mut rng).unwrap();
    let _client_confirm = alice.client_confirm(server_send).unwrap();
    assert_eq!(alice.shared_secret, bob.shared_secret);
}

#[test]
fn kyber_ake() {
    let mut rng = rand::thread_rng();
    let mut alice = Ake::new();
    let mut bob = Ake::new();
    let alice_keys = keypair(&mut rng);
    let bob_keys = keypair(&mut rng);
    let client_init = alice.client_init(&bob_keys.public, &mut rng);
    let server_send = bob
        .server_receive(client_init, &alice_keys.public, &bob_keys.secret, &mut rng)
        .unwrap();
    let _client_confirm = alice.client_confirm(server_send, &alice_keys.secret).unwrap();
    assert_eq!(alice.shared_secret, bob.shared_secret);
}
