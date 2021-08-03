pub mod c25519;
pub mod hash;
pub mod p521;
pub mod salsa;
pub mod poly1305;
pub mod balloon;
pub mod kbkdf;
pub mod random;
pub mod secret;

pub use aes_gmac_siv;

pub fn init() {
    // We always run gcrypt in "FIPS mode," but it doesn't count as fully compliant unless it's a FIPS-certified library.
    let _ = gcrypt::init_fips_mode(|_| -> Result<(), std::convert::Infallible> { Ok(()) });
}
