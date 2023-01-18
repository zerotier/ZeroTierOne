// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

pub mod aes;
pub mod aes_gmac_siv;
pub mod hash;
pub mod mimcvdf;
pub mod p384;
pub mod poly1305;
pub mod random;
pub mod salsa;
pub mod secret;
pub mod typestate;
pub mod x25519;

pub const ZEROES: [u8; 64] = [0_u8; 64];

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

extern "C" {
    fn OPENSSL_cleanse(ptr: *mut std::ffi::c_void, len: usize);
}

/// Destroy the contents of some memory
#[inline(always)]
pub fn burn(b: &mut [u8]) {
    unsafe { OPENSSL_cleanse(b.as_mut_ptr().cast(), b.len()) };
}
