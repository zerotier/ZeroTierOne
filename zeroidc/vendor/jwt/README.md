# JWT [![Build Status]][Github Actions] [![Latest Version]][crates.io] [![Documentation]][docs.rs]

[Build Status]: https://github.com/mikkyang/rust-jwt/actions/workflows/rust.yml/badge.svg?branch=master
[Github Actions]: https://github.com/mikkyang/rust-jwt/actions/workflows/rust.yml?query=branch%3Amaster
[Latest Version]: https://img.shields.io/crates/v/jwt.svg
[crates.io]: https://crates.io/crates/jwt
[Documentation]: https://img.shields.io/badge/rust-documentation-blue.svg
[docs.rs]: https://docs.rs/jwt

A JSON Web Token library.

### Only Claims

If you don't care about that header as long as the header is verified, signing
and verification can be done with just a few traits.

#### Signing

Claims can be any `serde::Serialize` type, usually derived with
`serde_derive`.

```rust
use hmac::{Hmac, Mac};
use jwt::SignWithKey;
use sha2::Sha256;
use std::collections::BTreeMap;

let key: Hmac<Sha256> = Hmac::new_from_slice(b"some-secret").unwrap();
let mut claims = BTreeMap::new();
claims.insert("sub", "someone");

let token_str = claims.sign_with_key(&key).unwrap();

assert_eq!(token_str, "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzb21lb25lIn0.5wwE1sBrs-vftww_BGIuTVDeHtc1Jsjo-fiHhDwR8m0");
```

#### Verification

Claims can be any `serde::Deserialize` type, usually derived with
`serde_derive`.

```rust
use hmac::{Hmac, Mac};
use jwt::VerifyWithKey;
use sha2::Sha256;
use std::collections::BTreeMap;

let key: Hmac<Sha256> = Hmac::new_from_slice(b"some-secret").unwrap();
let token_str = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzb21lb25lIn0.5wwE1sBrs-vftww_BGIuTVDeHtc1Jsjo-fiHhDwR8m0";

let claims: BTreeMap<String, String> = token_str.verify_with_key(&key).unwrap();

assert_eq!(claims["sub"], "someone");
```

### Header and Claims

If you need to customize the header, you can use the `Token` struct. For
convenience, a `Header` struct is provided for all of the commonly defined
fields, but any type that implements `JoseHeader` can be used.

#### Signing

Both header and claims have to implement `serde::Serialize`.

```rust
use hmac::{Hmac, Mac};
use jwt::{AlgorithmType, Header, SignWithKey, Token};
use sha2::Sha384;
use std::collections::BTreeMap;

let key: Hmac<Sha384> = Hmac::new_from_slice(b"some-secret").unwrap();
let header = Header {
    algorithm: AlgorithmType::Hs384,
    ..Default::default()
};
let mut claims = BTreeMap::new();
claims.insert("sub", "someone");

let token = Token::new(header, claims).sign_with_key(&key).unwrap();

assert_eq!(token.as_str(), "eyJhbGciOiJIUzM4NCJ9.eyJzdWIiOiJzb21lb25lIn0.WM_WnPUkHK6zm6Wz7zk1kmIxz990Te7nlDjQ3vzcye29szZ-Sj47rLNSTJNzpQd_");
```

#### Verification

Both header and claims have to implement `serde::Deserialize`.

```rust
use hmac::{Hmac, Mac};
use jwt::{AlgorithmType, Header, Token, VerifyWithKey};
use sha2::Sha384;
use std::collections::BTreeMap;

let key: Hmac<Sha384> = Hmac::new_from_slice(b"some-secret").unwrap();
let token_str = "eyJhbGciOiJIUzM4NCJ9.eyJzdWIiOiJzb21lb25lIn0.WM_WnPUkHK6zm6Wz7zk1kmIxz990Te7nlDjQ3vzcye29szZ-Sj47rLNSTJNzpQd_";

let token: Token<Header, BTreeMap<String, String>, _> = VerifyWithKey::verify_with_key(token_str, &key).unwrap();
let header = token.header();
let claims = token.claims();

assert_eq!(header.algorithm, AlgorithmType::Hs384);
assert_eq!(claims["sub"], "someone");
```

### Store
A `Store` can be used to represent a collection of keys indexed by key id. Right now, this is only automatically implemented for `BTreeMap` and `HashMap` with `Borrow<str>` keys. If specialization lands then it will be implemented for all `Index<&str>` traits as it was before.

For the trait `SignWithStore`, the key id will be automatically added to the header for bare claims.
Because claims do not have a way to specify key id, a tuple of key id and claims is necessary. For
tokens, the header's key id will be used to get the key.

For the trait `VerifyWithStore`, the key id from the deserialized header will be used to choose the key
to use.

```rust
use hmac::{Hmac, Mac};
use jwt::{Header, SignWithStore, Token, VerifyWithStore};
use sha2::Sha512;
use std::collections::BTreeMap;

let mut store: BTreeMap<_, Hmac<Sha512>> = BTreeMap::new();
store.insert("first_key", Hmac::new_from_slice(b"first").unwrap());
store.insert("second_key", Hmac::new_from_slice(b"second").unwrap());

let mut claims = BTreeMap::new();
claims.insert("sub", "someone");

let token_str = ("second_key", claims).sign_with_store(&store).unwrap();

assert_eq!(token_str, "eyJhbGciOiJIUzUxMiIsImtpZCI6InNlY29uZF9rZXkifQ.eyJzdWIiOiJzb21lb25lIn0.9gALQon5Mk8r4BjOZ2SJQlauGmT4WUhpN152x9dfKvkPON1VwEN09Id8vjQ0ABlfLJUTVNP36dsdrpYEZDLUcw");

let verified_token: Token<Header, BTreeMap<String, String>, _> = token_str.verify_with_store(&store).unwrap();
assert_eq!(verified_token.claims()["sub"], "someone");
assert_eq!(verified_token.header().key_id.as_ref().unwrap(), "second_key");
```

## Supported Algorithms

Pure Rust HMAC is supported through [RustCrypto](https://github.com/RustCrypto). Implementations of RSA and ECDSA signatures are supported through OpenSSL, which is not enabled by default. OpenSSL types must be wrapped in the [`PKeyWithDigest`](http://mikkyang.github.io/rust-jwt/doc/jwt/algorithm/openssl/struct.PKeyWithDigest.html) struct.

* HS256
* HS384
* HS512
* RS256
* RS384
* RS512
* ES256
* ES384
* ES512
