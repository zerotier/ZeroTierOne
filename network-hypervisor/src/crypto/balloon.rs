use std::convert::TryInto;
use std::mem::MaybeUninit;

#[inline(always)]
fn hash_int_le(sha: &mut crate::crypto::hash::SHA512, i: u64) {
    #[cfg(target_endian = "big")] {
        sha.update(&i.to_le_bytes());
    }
    #[cfg(target_endian = "little")] {
        sha.update(unsafe { &*(&i as *const u64).cast::<[u8; 8]>() });
    }
}

/// Compute balloon memory-hard hash using SHA-512.
/// SPACE_COST must be a multiple of 64. This is checked with an assertion.
/// DELTA is usually 3.
pub fn hash<const SPACE_COST: usize, const TIME_COST: usize, const DELTA: usize>(password: &[u8], salt: &[u8]) -> [u8; crate::crypto::hash::SHA512_HASH_SIZE] {
    assert_ne!(SPACE_COST, 0);
    assert_eq!((SPACE_COST % 64), 0);

    let mut buf: [u8; SPACE_COST] = unsafe { MaybeUninit::uninit().assume_init() };
    let zero64 = [0_u8; 8];

    /* Initial hash */
    let mut sha = crate::crypto::hash::SHA512::new();
    sha.update(&zero64); // 0 cnt
    sha.update(password);
    sha.update(salt);
    buf[0..64].copy_from_slice(sha.finish_get_ref());

    /* Expand */
    let mut cnt = 1_u64;
    let mut s: usize = 64;
    while s < SPACE_COST {
        sha.reset();
        hash_int_le(&mut sha, cnt);
        sha.update(&buf[(s - 64)..s]);
        let ss = s + 64;
        buf[s..ss].copy_from_slice(sha.finish_get_ref());
        s = ss;
        cnt += 1;
    }

    /* Mix */
    for t in 0..TIME_COST {
        sha.reset();
        hash_int_le(&mut sha, cnt);
        sha.update(&buf[(SPACE_COST - 64)..SPACE_COST]); // "previous" initially wraps back around to end
        sha.update(&buf[0..64]);
        buf[0..64].copy_from_slice(sha.finish_get_ref());
        cnt += 1;

        for i in 0..DELTA {
            sha.reset();
            hash_int_le(&mut sha, cnt);
            sha.update(salt);
            hash_int_le(&mut sha, t as u64);
            sha.update(&zero64); // s == 0
            hash_int_le(&mut sha, i as u64);
            cnt += 1;

            let other = sha.finish_get_ref();
            let other = ((u64::from_le_bytes(unsafe { *other.as_ptr().cast::<[u8; 8]>() }) % (SPACE_COST as u64 / 64)) * 64) as usize;

            sha.reset();
            hash_int_le(&mut sha, cnt);
            sha.update(&buf[0..64]);
            sha.update(&buf[other..(other + 64)]);
            buf[0..64].copy_from_slice(sha.finish_get_ref());
            cnt += 1;
        }

        let mut s = 64;
        while s < SPACE_COST {
            sha.reset();
            hash_int_le(&mut sha, cnt);
            sha.update(&buf[(s - 64)..s]);
            let ss = s + 64;
            sha.update(&buf[s..ss]);
            buf[s..ss].copy_from_slice(sha.finish_get_ref());
            cnt += 1;

            for i in 0..DELTA {
                sha.reset();
                hash_int_le(&mut sha, cnt);
                sha.update(salt);
                hash_int_le(&mut sha, t as u64);
                hash_int_le(&mut sha, s as u64);
                hash_int_le(&mut sha, i as u64);
                cnt += 1;

                let other = sha.finish_get_ref();
                let other = ((u64::from_le_bytes(unsafe { *other.as_ptr().cast::<[u8; 8]>() }) % (SPACE_COST as u64 / 64)) * 64) as usize;

                sha.reset();
                hash_int_le(&mut sha, cnt);
                sha.update(&buf[s..ss]);
                sha.update(&buf[other..(other + 64)]);
                buf[s..ss].copy_from_slice(sha.finish_get_ref());
                cnt += 1;
            }

            s = ss;
        }
    }

    /* Extract */
    buf[(SPACE_COST - 64)..SPACE_COST].try_into().unwrap()
}

/*
#[cfg(test)]
mod tests {
    #[test]
    fn balloon_test() {
        let start = std::time::SystemTime::now();
        let mut tmp = 0_u8;
        for _ in 0..100 {
            let foo = crate::crypto::balloon::hash::<16384, 3, 3>(&[1_u8], &[2_u8]);
            tmp = tmp.wrapping_add(foo[0]);
        }
        let duration = std::time::SystemTime::now().duration_since(start).unwrap();
        println!("Benchmark: {}ms per hash (junk to prevent optimizing out: {})", (duration.as_nanos() as f64 / 100.0) / 1000000.0, tmp);
    }
}
*/
