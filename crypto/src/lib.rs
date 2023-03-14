mod bn;
mod cipher_ctx;
mod ec;
mod error;

pub mod hash;
pub mod mimcvdf;
pub mod p384;
pub mod random;
pub mod secret;

pub mod poly1305;
pub mod salsa;
pub mod typestate;
pub mod x25519;

#[cfg(target_os = "macos")]
pub mod aes_fruity;
#[cfg(target_os = "macos")]
pub use aes_fruity as aes;

#[cfg(not(target_os = "macos"))]
pub mod aes_openssl;
#[cfg(not(target_os = "macos"))]
pub use aes_openssl as aes;

mod aes_tests;

#[cfg(target_os = "macos")]
pub mod aes_gmac_siv_fruity;
#[cfg(target_os = "macos")]
pub use aes_gmac_siv_fruity as aes_gmac_siv;

#[cfg(not(target_os = "macos"))]
pub mod aes_gmac_siv_openssl;
#[cfg(not(target_os = "macos"))]
pub use aes_gmac_siv_openssl as aes_gmac_siv;

use ctor::ctor;

#[ctor]
fn openssl_init() {
    println!("OpenSSL init()");
    ffi::init();
}

/// Constant time byte slice equality.
#[inline]
pub fn secure_eq<A: AsRef<[u8]> + ?Sized, B: AsRef<[u8]> + ?Sized>(a: &A, b: &B) -> bool {
    let (a, b) = (a.as_ref(), b.as_ref());
    if a.len() == b.len() {
        let mut x = 0u8;
        for (aa, bb) in a.iter().zip(b.iter()) {
            x |= *aa ^ *bb;
        }
        x == 0
    } else {
        false
    }
}

pub const ZEROES: [u8; 64] = [0_u8; 64];
