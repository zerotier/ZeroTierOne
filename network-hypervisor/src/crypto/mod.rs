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
use std::convert::Infallible;

static mut SALT64: u64 = 0;

pub fn init() {
    unsafe {
        // We always run gcrypt in "FIPS mode," but it doesn't count as fully compliant unless it's a FIPS-certified library.
        let _ = gcrypt::init_fips_mode(|_| -> Result<(), Infallible> { Ok(()) });

        while SALT64 == 0 {
            let mut tmp = 0_u64;
            gcrypt::rand::randomize(gcrypt::rand::Level::Strong, &mut *((&mut tmp as *mut u64).cast::<[u8; 8]>()));
            SALT64 = tmp;
        }
    }
}

pub fn salt64() -> u64 { unsafe { SALT64 } }
