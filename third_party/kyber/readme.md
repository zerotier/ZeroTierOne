

<p align="center">
  <img src="./kyber.png"/>
</p>


# Kyber
[![Build Status](https://github.com/Argyle-Cybersystems/kyber/actions/workflows/ci.yml/badge.svg)](https://github.com/Argyle-Cybersystems/kyber/actions)
[![Crates](https://img.shields.io/crates/v/pqc-kyber)](https://crates.io/crates/pqc-kyber)
[![NPM](https://img.shields.io/npm/v/pqc-kyber)](https://www.npmjs.com/package/pqc-kyber)
[![dependency status](https://deps.rs/repo/github/Argyle-Cybersystems/kyber/status.svg)](https://deps.rs/repo/github/Argyle-Cybersystems/kyber)
[![License](https://img.shields.io/badge/license-Apache-blue.svg)](https://github.com/Argyle-Cybersystems/kyber/blob/master/LICENSE)

A rust implementation of the Kyber algorithm, a post-quantum KEM that is a finalist in NIST's Post-Quantum Standardization Project.

This library:
* Is no_std compatible and needs no allocator, suitable for embedded devices. 
* Reference files contain no unsafe code and are written in pure rust.
* On x86_64 platforms uses an avx2 optimized version by default, which includes some assembly code taken from the C repo. 
* Compiles to WASM using wasm-bindgen and has a ready-to-use binary published on NPM.


See the [**features**](#features) section for different options regarding security levels and modes of operation. The default security setting is kyber764.

Please also read the [**security considerations**](#security-considerations) before use.

---

## Installation

In `Cargo.toml`:

```toml
[dependencies]
pqc_kyber = "0.2.0"
```

## Usage 

```rust
use pqc_kyber::*;
```

The higher level structs will be appropriate for most use-cases. 
Both unilateral or mutually authenticated key exchanges are possible.

---

### Unilaterally Authenticated Key Exchange
```rust
let mut rng = rand::thread_rng();

// Initialize the key exchange structs
let mut alice = Uake::new();
let mut bob = Uake::new();

// Generate Bob's Keypair
let bob_keys = keypair(&mut rng);

// Alice initiates key exchange
let client_init = alice.client_init(&bob_keys.public, &mut rng);

// Bob authenticates and responds
let server_response = bob.server_receive(
  client_init, &bob_keys.secret, &mut rng
)?;

// Alice decapsulates the shared secret
alice.client_confirm(server_response)?;

// Both key exchange structs now have the same shared secret
assert_eq!(alice.shared_secret, bob.shared_secret);
```

---

### Mutually Authenticated Key Exchange
Mutual authentication follows the same workflow but with additional keys passed to the functions:

```rust
let mut alice = Ake::new();
let mut bob = Ake::new();

let alice_keys = keypair(&mut rng);
let bob_keys = keypair(&mut rng);

let client_init = alice.client_init(&bob_keys.public, &mut rng);

let server_response = bob.server_receive(
  client_init, &alice_keys.public, &bob_keys.secret, &mut rng
)?;

alice.client_confirm(server_response, &alice_keys.secret)?;

assert_eq!(alice.shared_secret, bob.shared_secret);
```

---

### Key Encapsulation
Lower level functions for using the Kyber algorithm directly.
```rust
// Generate Keypair
let keys_bob = keypair(&mut rng);

// Alice encapsulates a shared secret using Bob's public key
let (ciphertext, shared_secret_alice) = encapsulate(&keys_bob.public, &mut rng)?;

// Bob decapsulates a shared secret using the ciphertext sent by Alice 
let shared_secret_bob = decapsulate(&ciphertext, &keys_bob.secret)?;

assert_eq!(shared_secret_alice, shared_secret_bob);
```

---

## Errors
The KyberError enum handles errors. It has two variants:

* **InvalidInput** - One or more inputs to a function are incorrectly sized. A possible cause of this is two parties using different security levels while trying to negotiate a key exchange.

* **Decapsulation** - The ciphertext was unable to be authenticated. The shared secret was not decapsulated.

---

## Features

If no security level is specified then kyber764 is used by default as recommended by the authors. It is roughly equivalent to AES-196.  Apart from the two security levels, all other features can be combined as needed. For example:

```toml
[dependencies]
pqc_kyber = {version = "0.2.0", features = ["kyber512", "90s", "reference"]}
```


| Feature   | Description                                                                                                                                                                |
|-----------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| kyber512  | Enables kyber512 mode, with a security level roughly equivalent to AES-128.                                                                                                |
| kyber1024 | Enables kyber1024 mode, with a security level roughly equivalent to AES-256.  A compile-time error is raised if more than one security level is specified.                 |
| 90s       | Uses SHA2 and AES in counter mode as a replacement for SHAKE. This can provide hardware speedups in some cases. |
| reference | On x86_64 platforms the optimized version is used by default. Enabling this feature will force usage of the reference codebase. This flag is redundant on other architectures and has no effect. |
| wasm      | For compiling to WASM targets.                                                                                                                                     |

---

## Testing

The [run_all_tests](tests/run_all_tests.sh) script will traverse all possible codepaths by running a matrix of the security levels and variants.

Known Answer Tests require deterministic rng seeds, enable the `KATs` feature to run them, you must also specify the module as noted below. Using this feature outside of `cargo test` will result in a compile-time error.

```bash
# This example runs all KATs for kyber512-90s, note `--test kat` is needed here.
cargo test --test kat --features "KATs kyber512 90s"
```

The test vector files are quite large, you will need to build them yourself from the C reference code. There's a helper script to do this [here](./tests/KATs/build_kats.sh). 

See the [testing readme](./tests/readme.md) for more comprehensive info.

---

## Benchmarking

Uses criterion for benchmarking. If you have GNUPlot installed it will generate statistical graphs in `target/criterion/`.

See the [benchmarking readme](./benches/readme.md) for information on correct usage.

---

## Fuzzing

The fuzzing suite uses honggfuzz, installation and instructions are on the [fuzzing](./fuzz/readme.md) page. 

---

## WebAssembly

This library has been compiled and published as a WASM binary package. Usage instructions are published on npm:

https://www.npmjs.com/package/pqc-kyber

Which is also located here in the [wasm readme](./pkg/README.md)

To install:

```
npm i pqc-kyber
```

To use this library for web assembly purposes you'll need the `wasm` feature enabled.

```toml
[dependencies]
pqc-kyber = {version = "0.2.0", features = ["wasm"]
```

You will also need `wasm-pack` and `wasm32-unknown-unknown` or `wasm32-unknown-emscripten` toolchains installed

To build include the feature flag:

```shell
wasm-pack build -- --features wasm
```

---

## Security Considerations
The NIST post quantum standardisation project is still ongoing and changes may still be made to the underlying reference code at any time. 

While much care has been taken porting from the C reference codebase, this library has not undergone any third-party security auditing nor can any guarantees be made about the potential for underlying vulnerabilities in LWE cryptography or potential side-channel attacks arising from this implementation.

Please use at your own risk.

---

## About

Kyber is an IND-CCA2-secure key encapsulation mechanism (KEM), whose security is based on the hardness of solving the learning-with-errors (LWE) problem over module lattices. It is one of the round 3 finalist algorithms submitted to the [NIST post-quantum cryptography project](https://csrc.nist.gov/Projects/Post-Quantum-Cryptography).

The official website: https://pq-crystals.org/kyber/

Authors of the Kyber Algorithm: 

* Roberto Avanzi, ARM Limited (DE)
* Joppe Bos, NXP Semiconductors (BE)
* Léo Ducas, CWI Amsterdam (NL)
* Eike Kiltz, Ruhr University Bochum (DE)
* Tancrède Lepoint, SRI International (US)
* Vadim Lyubashevsky, IBM Research Zurich (CH)
* John M. Schanck, University of Waterloo (CA)
* Peter Schwabe, Radboud University (NL)
* Gregor Seiler, IBM Research Zurich (CH)
* Damien Stehle, ENS Lyon (FR)

---

### Contributing 

Contributions welcome. For pull requests create a feature fork and submit it to the development branch. More information is available on the [contributing page](./contributing.md)


