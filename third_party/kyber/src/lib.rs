//! # Kyber
//!
//! A rust implementation of the Kyber algorithm
//!
//! This library:
//! * Is no_std compatible and uses no allocations, suitable for embedded devices.
//! * The reference files contain no unsafe code.
//! * On x86_64 platforms uses an optimized avx2 version by default.
//! * Compiles to WASM using wasm-bindgen.
//!
//! ## Features
//! If no security level is set then Kyber764 is used, this is roughly equivalent to AES-196. See below for setting other levels.
//! A compile-time error is raised if more than one level is specified. Besides that all other features can be mixed as needed:
//!
//! | Feature   | Description                                                                                                                                                                |
//! |-----------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
//! | kyber512  | Enables kyber512 mode, with a security level roughly equivalent to AES-128.                                                                                                |
//! | kyber1024 | Enables kyber1024 mode, with a security level roughly equivalent to AES-256.                   |
//! | 90s       | 90's mode uses SHA2 and AES-CTR as a replacement for SHAKE. This may provide hardware speedups on certain architectures.                                                           |
//! | reference | On x86_64 platforms the optimized version is used by default. Enabling this feature will force usage of the reference codebase. This is unnecessary on other architectures |
//! | wasm      | For compiling to WASM targets. |
//!
//! ## Usage
//!
//! ```
//! use pqc_kyber::*;
//! ```
//!
//! The higher level structs will be appropriate for most use-cases.
//! Both [unilateral](struct.Uake.html) or [mutually](struct.Ake.html) authenticated key exchanges are possible.
//!
//! #### Unilaterally Authenticated Key Exchange
//! ```
//! # use pqc_kyber::*;
//! # fn main() -> Result<(),KyberError> {
//! let mut rng = rand::thread_rng();
//!
//! // Initialize the key exchange structs
//! let mut alice = Uake::new();
//! let mut bob = Uake::new();
//!
//! // Generate Keypairs
//! let alice_keys = keypair(&mut rng);
//! let bob_keys = keypair(&mut rng);
//!
//! // Alice initiates key exchange
//! let client_init = alice.client_init(&bob_keys.public, &mut rng);
//!
//! // Bob authenticates and responds
//! let server_send = bob.server_receive(
//!   client_init, &bob_keys.secret, &mut rng
//! )?;
//!
//! // Alice decapsulates the shared secret
//! alice.client_confirm(server_send)?;
//!
//! // Both key exchange structs now have the shared secret
//! assert_eq!(alice.shared_secret, bob.shared_secret);
//! # Ok(()) }
//! ```
//!
//! #### Mutually Authenticated Key Exchange
//! Mutual authentication follows the same workflow but with additional keys passed to the functions:
//!
//! ```
//! # use pqc_kyber::*;
//! # fn main() -> Result<(),KyberError> {
//! # let mut rng = rand::thread_rng();
//! let mut alice = Ake::new();
//! let mut bob = Ake::new();
//!
//! let alice_keys = keypair(&mut rng);
//! let bob_keys = keypair(&mut rng);
//!
//! let client_init = alice.client_init(&bob_keys.public, &mut rng);
//!
//! let server_send = bob.server_receive(
//!   client_init, &alice_keys.public, &bob_keys.secret, &mut rng
//! )?;
//!
//! alice.client_confirm(server_send, &alice_keys.secret)?;
//!
//! assert_eq!(alice.shared_secret, bob.shared_secret);
//! # Ok(()) }
//! ```
//!
//! ##### Key Encapsulation
//! Lower level functions for using the Kyber algorithm directly.
//! ```
//! # use pqc_kyber::*;
//! # fn main() -> Result<(),KyberError> {
//! # let mut rng = rand::thread_rng();
//! // Generate Keypair
//! let keys_bob = keypair(&mut rng);
//!
//! // Alice encapsulates a shared secret using Bob's public key
//! let (ciphertext, shared_secret_alice) = encapsulate(&keys_bob.public, &mut rng)?;
//!
//! // Bob decapsulates a shared secret using the ciphertext sent by Alice
//! let shared_secret_bob = decapsulate(&ciphertext, &keys_bob.secret)?;
//!
//! assert_eq!(shared_secret_alice, shared_secret_bob);
//! # Ok(()) }
//! ```
//!
//! ## Errors
//! The [KyberError](enum.KyberError.html) enum handles errors. It has two variants:
//!
//! * **InvalidInput** - One or more byte inputs to a function are incorrectly sized. A likely cause of
//! this is two parties using different security levels while trying to negotiate a key exchange.
//!
//! * **Decapsulation** - The ciphertext was unable to be authenticated. The shared secret was not decapsulated  

//#![no_std]
#![allow(clippy::many_single_char_names)]

// Prevent usage of mutually exclusive features
#[cfg(all(feature = "kyber1024", feature = "kyber512"))]
compile_error!("Only one security level can be specified");

#[cfg(all(target_arch = "x86_64", not(feature = "reference")))]
mod avx2;
#[cfg(all(target_arch = "x86_64", not(feature = "reference")))]
use avx2::*;

#[cfg(any(not(target_arch = "x86_64"), feature = "reference"))]
mod reference;
#[cfg(any(not(target_arch = "x86_64"), feature = "reference"))]
use reference::*;

mod api;
mod error;
mod kem;
mod kex;
mod params;
mod rng;
mod symmetric;

pub use api::*;
pub use error::KyberError;
pub use kex::*;
pub use params::*;
pub use rand_core::{CryptoRng, RngCore};

// Feature hack to expose private functions for the Known Answer Tests
// and fuzzing. Will fail to compile if used outside `cargo test` or
// the fuzz binaries.
#[cfg(any(feature = "KATs", fuzzing))]
pub use kem::*;
