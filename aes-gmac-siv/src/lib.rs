#[cfg(any(target_os = "macos", target_os = "ios"))]
mod impl_macos;

#[cfg(not(any(target_os = "macos", target_os = "ios", target_arch = "s390x", target_arch = "powerpc64le", target_arch = "powerpc64")))]
mod impl_gcrypt;

#[cfg(all(not(any(target_os = "macos", target_os = "ios")), any(target_arch = "s390x", target_arch = "powerpc64le", target_arch = "powerpc64")))]
mod impl_openssl;

#[cfg(any(target_os = "macos", target_os = "ios"))]
pub use impl_macos::{AesCtr, AesGmacSiv};

#[cfg(not(any(target_os = "macos", target_os = "ios", target_arch = "s390x", target_arch = "powerpc64le", target_arch = "powerpc64")))]
pub use impl_gcrypt::AesGmacSiv;

#[cfg(all(not(any(target_os = "macos", target_os = "ios")), any(target_arch = "s390x", target_arch = "powerpc64le", target_arch = "powerpc64")))]
pub use impl_openssl::AesGmacSiv;

pub(crate) const ZEROES: [u8; 16] = [0_u8; 16];

#[cfg(test)]
mod tests {
    use crate::AesGmacSiv;
    use std::time::SystemTime;
    use sha2::Digest;

    fn to_hex(b: &[u8]) -> String {
        let mut s = String::new();
        for c in b.iter() {
            s = format!("{}{:0>2x}", s, *c);
        }
        s
    }

    #[test]
    fn encrypt_decrypt() {
        let aes_key_0: [u8; 32] = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32];
        let aes_key_1: [u8; 32] = [2,3,4,5,6,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32];
        let iv: [u8; 8] = [0,1,2,3,4,5,6,7];

        let mut buf = [0_u8; 12345];
        for i in 1..12345 {
            buf[i] = i as u8;
        }

        let mut c = AesGmacSiv::new(&aes_key_0, &aes_key_1);

        for _ in 0..256 {
            c.reset();
            c.encrypt_init(&iv);
            c.encrypt_first_pass(&buf);
            c.encrypt_first_pass_finish();
            c.encrypt_second_pass_in_place(&mut buf);
            let tag = c.encrypt_second_pass_finish().clone();
            let sha = sha2::Sha384::digest(&buf).to_vec();
            let sha = to_hex(sha.as_slice());
            if sha != "b5c0997f5a0e31748dfd3a22baa4bf257b1ebf2c2ecb78a68360fa5a5f0763aa5b8fde8be502255c82d937d8adba11eb" {
                panic!("encrypt result hash check failed! {}", sha);
            }
            //println!("Encrypt OK, tag: {}, hash: {}", to_hex(&tag), sha);

            c.reset();
            c.decrypt_init(&tag);
            c.decrypt_in_place(&mut buf);
            if !c.decrypt_finish() {
                panic!("decrypt tag check failed!");
            }
            for i in 1..12345 {
                if buf[i] != (i & 0xff) as u8 {
                    panic!("decrypt data check failed!");
                }
            }
            //println!("Decrypt OK");
        }
        println!("Encrypt/decrypt test OK");

        let benchmark_iterations: usize = 50000;
        let start = SystemTime::now();
        for _ in 0..benchmark_iterations {
            c.reset();
            c.encrypt_init(&iv);
            c.encrypt_first_pass(&buf);
            c.encrypt_first_pass_finish();
            c.encrypt_second_pass_in_place(&mut buf);
            let _ = c.encrypt_second_pass_finish();
        }
        let duration = SystemTime::now().duration_since(start).unwrap();
        println!("Encrypt benchmark: {} MiB/sec", (((benchmark_iterations * buf.len()) as f64) / 1048576.0) / duration.as_secs_f64());
        let start = SystemTime::now();
        for _ in 0..benchmark_iterations {
            c.reset();
            c.decrypt_init(&buf[0..16]); // we don't care if decryption is successful to benchmark, so anything will do
            c.decrypt_in_place(&mut buf);
            c.decrypt_finish();
        }
        let duration = SystemTime::now().duration_since(start).unwrap();
        println!("Decrypt benchmark: {} MiB/sec", (((benchmark_iterations * buf.len()) as f64) / 1048576.0) / duration.as_secs_f64());
    }
}
