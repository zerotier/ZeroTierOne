#[cfg(any(target_os = "macos", target_os = "ios"))]
mod impl_macos;
#[cfg(not(any(target_os = "macos", target_os = "ios")))]
mod impl_gcrypt;

#[cfg(any(target_os = "macos", target_os = "ios"))]
pub use impl_macos::AesGmacSiv;
#[cfg(not(any(target_os = "macos", target_os = "ios")))]
pub use impl_gcrypt::AesGmacSiv;

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
        let aes_key: [u8; 32] = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32];
        let iv: [u8; 8] = [0,1,2,3,4,5,6,7];

        let mut buf = [0_u8; 12345];
        for i in 1..12345 {
            buf[i] = i as u8;
        }

        let mut c = AesGmacSiv::new(&aes_key);

        for _ in 0..256 {
            c.reset();
            c.encrypt_init(&iv);
            c.encrypt_first_pass(&buf);
            c.encrypt_first_pass_finish();
            c.encrypt_second_pass_in_place(&mut buf);
            let tag = c.encrypt_second_pass_finish().clone();
            let sha = sha2::Sha384::digest(&buf).to_vec();
            let sha = to_hex(sha.as_slice());
            if sha != "f455fa8a1a6badaeccdefe573a10d5d79eb7f4009b84dff3d37f9f1e95ee2b0ba6149737c0701d5ef75f58f793174d3d" {
                panic!("encrypt result hash check failed!");
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
