#![allow(
    dead_code,
    mutable_transmutes,
    non_camel_case_types,
    non_snake_case,
    non_upper_case_globals,
    unused_assignments,
    unused_mut
)]

pub const P384_PUBLIC_KEY_SIZE: usize = 49;
pub const P384_SECRET_KEY_SIZE: usize = 48;
pub const P384_ECDSA_SIGNATURE_SIZE: usize = 96;
pub const P384_ECDH_SHARED_SECRET_SIZE: usize = 48;

// Version using OpenSSL's ECC
#[cfg(not(target_feature = "builtin_nist_ecc"))]
mod openssl_based {
    use std::convert::TryInto;
    use std::os::raw::{c_int, c_ulong, c_void};
    use std::{mem, ptr};

    use crate::bn::{BigNum, BigNumContext};
    use crate::ec::{EcGroup, EcKey, EcPoint, EcPointRef};
    use foreign_types::{ForeignType, ForeignTypeRef};
    use lazy_static::lazy_static;

    use crate::error::cvt_p;
    use crate::hash::SHA384;
    use crate::secret::Secret;
    use crate::secure_eq;

    use super::{P384_ECDH_SHARED_SECRET_SIZE, P384_ECDSA_SIGNATURE_SIZE, P384_PUBLIC_KEY_SIZE, P384_SECRET_KEY_SIZE};

    //#[link(name="crypto")]
    extern "C" {
        fn ECDH_compute_key(out: *mut c_void, outlen: c_ulong, pub_key: *mut c_void, ecdh: *mut c_void, kdf: *const c_void) -> c_int;
    }

    lazy_static! {
        pub(crate) static ref GROUP_P384: EcGroup = unsafe { EcGroup::from_ptr(cvt_p(ffi::EC_GROUP_new_by_curve_name(ffi::NID_secp384r1)).unwrap()) };
    }

    /// A NIST P-384 ECDH/ECDSA public key.
    #[derive(Clone)]
    pub struct P384PublicKey {
        key: EcKey,
        bytes: [u8; 49],
    }

    impl P384PublicKey {
        fn new_from_point(key: &EcPointRef) -> Self {
            let mut bnc = BigNumContext::new().unwrap();
            let kb = key
                .to_bytes(&GROUP_P384, ffi::point_conversion_form_t::POINT_CONVERSION_COMPRESSED, &bnc)
                .unwrap();
            let mut bytes = [0_u8; 49];
            bytes[(49 - kb.len())..].copy_from_slice(kb.as_slice());
            Self {
                key: EcKey::from_public_key(&GROUP_P384, key).unwrap(),
                bytes,
            }
        }

        pub fn from_bytes(b: &[u8]) -> Option<P384PublicKey> {
            if b.len() == 49 {
                let mut bnc = BigNumContext::new().unwrap();
                if let Ok(point) = EcPoint::from_bytes(&GROUP_P384, b, &mut bnc) {
                    if point.is_on_curve(&GROUP_P384, &bnc).unwrap_or(false) {
                        if let Ok(key) = EcKey::from_public_key(&GROUP_P384, &point) {
                            return Some(Self { key, bytes: b.try_into().unwrap() });
                        }
                    }
                }
            }
            return None;
        }

        pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
            if signature.len() == 96 {
                let r = BigNum::from_slice(&signature[0..48]);
                let s = BigNum::from_slice(&signature[48..96]);
                if let (Ok(r), Ok(s)) = (r, s) {
                    let z = BigNum::from_u32(0).unwrap();
                    // Check that r and s are >=1 just in case the OpenSSL version or an OpenSSL API lookalike is
                    // vulnerable to this, since a bunch of vulnerabilities involving zero r/s just made the rounds.
                    if r.gt(&z) && s.gt(&z) {
                        unsafe {
                            let sig = ffi::ECDSA_SIG_new();
                            if !sig.is_null() {
                                if ffi::ECDSA_SIG_set0(sig, r.as_ptr(), s.as_ptr()) == 1 {
                                    mem::forget((r, s));

                                    let data = &SHA384::hash(msg);

                                    return ffi::ECDSA_do_verify(data.as_ptr(), data.len() as c_int, sig, self.key.as_ptr()) == 1;
                                }
                            }
                        }
                    }
                }
            }
            return false;
        }

        pub fn as_bytes(&self) -> &[u8; 49] {
            &self.bytes
        }
    }

    impl PartialEq for P384PublicKey {
        fn eq(&self, other: &Self) -> bool {
            self.bytes == other.bytes
        }
    }

    unsafe impl Send for P384PublicKey {}

    unsafe impl Sync for P384PublicKey {}

    /// A NIST P-384 ECDH/ECDSA public/private key pair.
    #[derive(Clone)]
    pub struct P384KeyPair {
        pair: EcKey,
        public: P384PublicKey,
    }

    impl P384KeyPair {
        pub fn generate() -> P384KeyPair {
            let pair = EcKey::generate(&GROUP_P384).unwrap(); // failure implies a serious problem
            let public = P384PublicKey::new_from_point(pair.public_key());
            Self { pair, public }
        }

        pub fn from_bytes(public_bytes: &[u8], secret_bytes: &[u8]) -> Option<P384KeyPair> {
            if public_bytes.len() == 49 && secret_bytes.len() == 48 {
                if let Some(public) = P384PublicKey::from_bytes(public_bytes) {
                    if let Ok(private) = BigNum::from_slice(secret_bytes) {
                        if let Ok(pair) = EcKey::from_private_components(&GROUP_P384, &private, public.key.public_key()) {
                            if pair.check_key().is_ok() {
                                return Some(Self { pair, public });
                            }
                        }
                    }
                }
            }
            return None;
        }

        pub fn public_key(&self) -> &P384PublicKey {
            &self.public
        }

        pub fn public_key_bytes(&self) -> &[u8; P384_PUBLIC_KEY_SIZE] {
            &self.public.bytes
        }

        pub fn secret_key_bytes(&self) -> Secret<P384_SECRET_KEY_SIZE> {
            let mut tmp: Secret<P384_SECRET_KEY_SIZE> = Secret::default();
            let size = self.pair.private_key().to_bytes(&mut tmp.0).unwrap();
            tmp.0.copy_within(..size, P384_SECRET_KEY_SIZE - size);
            tmp
        }

        /// Sign a message with ECDSA/SHA384.
        pub fn sign(&self, msg: &[u8]) -> [u8; P384_ECDSA_SIGNATURE_SIZE] {
            let data = &SHA384::hash(msg);
            unsafe {
                let sig = ffi::ECDSA_do_sign(data.as_ptr(), data.len() as c_int, self.pair.as_ref().as_ptr());
                assert!(!sig.is_null());

                let mut r = ptr::null();
                let mut s = ptr::null();
                ffi::ECDSA_SIG_get0(sig, &mut r, &mut s);
                let r_len = ((ffi::BN_num_bits(r) + 7) / 8) as usize;
                let s_len = ((ffi::BN_num_bits(s) + 7) / 8) as usize;
                const CAP: usize = P384_ECDSA_SIGNATURE_SIZE / 2;
                assert!(r_len > 0 && s_len > 0 && r_len <= CAP && s_len <= CAP);

                let mut b = [0_u8; P384_ECDSA_SIGNATURE_SIZE];
                ffi::BN_bn2bin(r, b[(CAP - r_len)..CAP].as_mut_ptr());
                ffi::BN_bn2bin(s, b[(P384_ECDSA_SIGNATURE_SIZE - s_len)..P384_ECDSA_SIGNATURE_SIZE].as_mut_ptr());
                b
            }
        }

        /// Perform ECDH key agreement, returning the raw (un-hashed!) ECDH secret.
        ///
        /// This secret should not be used directly. It should be hashed and perhaps used in a KDF.
        pub fn agree(&self, other_public: &P384PublicKey) -> Option<Secret<P384_ECDH_SHARED_SECRET_SIZE>> {
            unsafe {
                let mut s: Secret<P384_ECDH_SHARED_SECRET_SIZE> = Secret::default();
                if ECDH_compute_key(
                    s.0.as_mut_ptr().cast(),
                    48,
                    other_public.key.public_key().as_ptr().cast(),
                    self.pair.as_ptr().cast(),
                    ptr::null(),
                ) == 48
                {
                    Some(s)
                } else {
                    None
                }
            }
        }
    }

    impl PartialEq for P384KeyPair {
        fn eq(&self, other: &Self) -> bool {
            self.pair.private_key().eq(other.pair.private_key()) && secure_eq(&self.public.bytes, &other.public.bytes)
        }
    }

    impl Eq for P384KeyPair {}

    unsafe impl Send for P384KeyPair {}

    unsafe impl Sync for P384KeyPair {}
}

// This is small and relatively fast but may not be constant time and hasn't been well audited, so we don't
// use it by default. It's left here though in case it proves useful in the future on embedded systems.
#[cfg(target_feature = "builtin_nist_ecc")]
mod builtin {
    use crate::hash::SHA384;
    use crate::secret::Secret;

    // EASY-ECC by Kenneth MacKay
    // https://github.com/esxgx/easy-ecc (no longer there, but search GitHub for forks)
    //
    // Translated directly from C to Rust using https://c2rust.com and then hacked a bit
    // to eliminate some dependencies. The translated code still has a lot of gratuitous
    // "as"es but they're not consequential.
    //
    // It inherits its original BSD 2-Clause license, not ZeroTier's license.

    pub mod libc {
        pub type c_uchar = u8;
        pub type c_ulong = u64;
        pub type c_long = i64;
        pub type c_uint = u32;
        pub type c_int = i32;
        pub type c_ulonglong = u64;
        pub type c_longlong = i64;
    }

    pub type uint8_t = libc::c_uchar;
    pub type uint64_t = libc::c_ulong;
    pub type uint = libc::c_uint;
    pub type uint128_t = u128;
    pub struct EccPoint {
        pub x: [u64; 6],
        pub y: [u64; 6],
    }
    static mut curve_p: [uint64_t; 6] = [
        0xffffffff as libc::c_uint as uint64_t,
        0xffffffff00000000 as libc::c_ulong,
        0xfffffffffffffffe as libc::c_ulong,
        0xffffffffffffffff as libc::c_ulong,
        0xffffffffffffffff as libc::c_ulong,
        0xffffffffffffffff as libc::c_ulong,
    ];
    static mut curve_b: [uint64_t; 6] = [
        0x2a85c8edd3ec2aef as libc::c_long as uint64_t,
        0xc656398d8a2ed19d as libc::c_ulong,
        0x314088f5013875a as libc::c_long as uint64_t,
        0x181d9c6efe814112 as libc::c_long as uint64_t,
        0x988e056be3f82d19 as libc::c_ulong,
        0xb3312fa7e23ee7e4 as libc::c_ulong,
    ];
    static mut curve_G: EccPoint = {
        let mut init = EccPoint {
            x: [
                0x3a545e3872760ab7 as libc::c_long as uint64_t,
                0x5502f25dbf55296c as libc::c_long as uint64_t,
                0x59f741e082542a38 as libc::c_long as uint64_t,
                0x6e1d3b628ba79b98 as libc::c_long as uint64_t,
                0x8eb1c71ef320ad74 as libc::c_ulong,
                0xaa87ca22be8b0537 as libc::c_ulong,
            ],
            y: [
                0x7a431d7c90ea0e5f as libc::c_long as uint64_t,
                0xa60b1ce1d7e819d as libc::c_long as uint64_t,
                0xe9da3113b5f0b8c0 as libc::c_ulong,
                0xf8f41dbd289a147c as libc::c_ulong,
                0x5d9e98bf9292dc29 as libc::c_long as uint64_t,
                0x3617de4a96262c6f as libc::c_long as uint64_t,
            ],
        };
        init
    };
    static mut curve_n: [uint64_t; 6] = [
        0xecec196accc52973 as libc::c_ulong,
        0x581a0db248b0a77a as libc::c_long as uint64_t,
        0xc7634d81f4372ddf as libc::c_ulong,
        0xffffffffffffffff as libc::c_ulong,
        0xffffffffffffffff as libc::c_ulong,
        0xffffffffffffffff as libc::c_ulong,
    ];

    unsafe fn getRandomNumber(mut p_vli: *mut uint64_t) -> libc::c_int {
        crate::random::fill_bytes_secure(&mut *std::ptr::slice_from_raw_parts_mut(p_vli.cast(), 48));
        return 1 as libc::c_int;
    }

    unsafe fn vli_clear(mut p_vli: *mut uint64_t) {
        let mut i: uint = 0;
        i = 0 as libc::c_int as uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            *p_vli.offset(i as isize) = 0 as libc::c_int as uint64_t;
            i = i.wrapping_add(1)
        }
    }
    /* Returns 1 if p_vli == 0, 0 otherwise. */

    unsafe fn vli_isZero(mut p_vli: *mut uint64_t) -> libc::c_int {
        let mut i: uint = 0;
        i = 0 as libc::c_int as uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            if *p_vli.offset(i as isize) != 0 {
                return 0 as libc::c_int;
            }
            i = i.wrapping_add(1)
        }
        return 1 as libc::c_int;
    }
    /* Returns nonzero if bit p_bit of p_vli is set. */

    unsafe fn vli_testBit(mut p_vli: *mut uint64_t, mut p_bit: uint) -> uint64_t {
        return *p_vli.offset(p_bit.wrapping_div(64 as libc::c_int as libc::c_uint) as isize)
            & (1 as libc::c_int as uint64_t) << p_bit.wrapping_rem(64 as libc::c_int as libc::c_uint);
    }
    /* Counts the number of 64-bit "digits" in p_vli. */

    unsafe fn vli_numDigits(mut p_vli: *mut uint64_t) -> uint {
        let mut i: libc::c_int = 0;
        /* Search from the end until we find a non-zero digit.
        We do it in reverse because we expect that most digits will be nonzero. */
        i = 48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int;
        while i >= 0 as libc::c_int && *p_vli.offset(i as isize) == 0 as libc::c_int as libc::c_ulong {
            i -= 1
        }
        return (i + 1 as libc::c_int) as uint;
    }
    /* Counts the number of bits required for p_vli. */

    unsafe fn vli_numBits(mut p_vli: *mut uint64_t) -> uint {
        let mut i: uint = 0;
        let mut l_digit: uint64_t = 0;
        let mut l_numDigits: uint = vli_numDigits(p_vli);
        if l_numDigits == 0 as libc::c_int as libc::c_uint {
            return 0 as libc::c_int as uint;
        }
        l_digit = *p_vli.offset(l_numDigits.wrapping_sub(1 as libc::c_int as libc::c_uint) as isize);
        i = 0 as libc::c_int as uint;
        while l_digit != 0 {
            l_digit >>= 1 as libc::c_int;
            i = i.wrapping_add(1)
        }
        return l_numDigits
            .wrapping_sub(1 as libc::c_int as libc::c_uint)
            .wrapping_mul(64 as libc::c_int as libc::c_uint)
            .wrapping_add(i);
    }
    /* Sets p_dest = p_src. */

    unsafe fn vli_set(mut p_dest: *mut uint64_t, mut p_src: *mut uint64_t) {
        let mut i: uint = 0;
        i = 0 as libc::c_int as uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            *p_dest.offset(i as isize) = *p_src.offset(i as isize);
            i = i.wrapping_add(1)
        }
    }
    /* Returns sign of p_left - p_right. */

    unsafe fn vli_cmp(mut p_left: *mut uint64_t, mut p_right: *mut uint64_t) -> libc::c_int {
        let mut i: libc::c_int = 0;
        i = 48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int;
        while i >= 0 as libc::c_int {
            if *p_left.offset(i as isize) > *p_right.offset(i as isize) {
                return 1 as libc::c_int;
            } else {
                if *p_left.offset(i as isize) < *p_right.offset(i as isize) {
                    return -(1 as libc::c_int);
                }
            }
            i -= 1
        }
        return 0 as libc::c_int;
    }
    /* Computes p_result = p_in << c, returning carry. Can modify in place (if p_result == p_in). 0 < p_shift < 64. */

    unsafe fn vli_lshift(mut p_result: *mut uint64_t, mut p_in: *mut uint64_t, mut p_shift: uint) -> uint64_t {
        let mut l_carry: uint64_t = 0 as libc::c_int as uint64_t;
        let mut i: uint = 0;
        i = 0 as libc::c_int as uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            let mut l_temp: uint64_t = *p_in.offset(i as isize);
            *p_result.offset(i as isize) = l_temp << p_shift | l_carry;
            l_carry = l_temp >> (64 as libc::c_int as libc::c_uint).wrapping_sub(p_shift);
            i = i.wrapping_add(1)
        }
        return l_carry;
    }
    /* Computes p_vli = p_vli >> 1. */

    unsafe fn vli_rshift1(mut p_vli: *mut uint64_t) {
        let mut l_end: *mut uint64_t = p_vli;
        let mut l_carry: uint64_t = 0 as libc::c_int as uint64_t;
        p_vli = p_vli.offset((48 as libc::c_int / 8 as libc::c_int) as isize);
        loop {
            let fresh0 = p_vli;
            p_vli = p_vli.offset(-1);
            if !(fresh0 > l_end) {
                break;
            }
            let mut l_temp: uint64_t = *p_vli;
            *p_vli = l_temp >> 1 as libc::c_int | l_carry;
            l_carry = l_temp << 63 as libc::c_int
        }
    }
    /* Computes p_result = p_left + p_right, returning carry. Can modify in place. */

    unsafe fn vli_add(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t) -> uint64_t {
        let mut l_carry: uint64_t = 0 as libc::c_int as uint64_t;
        let mut i: uint = 0;
        i = 0 as libc::c_int as uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            let mut l_sum: uint64_t = (*p_left.offset(i as isize))
                .wrapping_add(*p_right.offset(i as isize))
                .wrapping_add(l_carry);
            if l_sum != *p_left.offset(i as isize) {
                l_carry = (l_sum < *p_left.offset(i as isize)) as libc::c_int as uint64_t
            }
            *p_result.offset(i as isize) = l_sum;
            i = i.wrapping_add(1)
        }
        return l_carry;
    }
    /* Computes p_result = p_left - p_right, returning borrow. Can modify in place. */

    unsafe fn vli_sub(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t) -> uint64_t {
        let mut l_borrow: uint64_t = 0 as libc::c_int as uint64_t;
        let mut i: uint = 0;
        i = 0 as libc::c_int as uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            let mut l_diff: uint64_t = (*p_left.offset(i as isize))
                .wrapping_sub(*p_right.offset(i as isize))
                .wrapping_sub(l_borrow);
            if l_diff != *p_left.offset(i as isize) {
                l_borrow = (l_diff > *p_left.offset(i as isize)) as libc::c_int as uint64_t
            }
            *p_result.offset(i as isize) = l_diff;
            i = i.wrapping_add(1)
        }
        return l_borrow;
    }
    /* Computes p_result = p_left * p_right. */

    unsafe fn vli_mult(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t) {
        let mut r01: uint128_t = 0 as libc::c_int as uint128_t;
        let mut r2: uint64_t = 0 as libc::c_int as uint64_t;
        let mut i: uint = 0;
        let mut k: uint = 0;
        /* Compute each digit of p_result in sequence, maintaining the carries. */
        k = 0 as libc::c_int as uint;
        while k < (48 as libc::c_int / 8 as libc::c_int * 2 as libc::c_int - 1 as libc::c_int) as libc::c_uint {
            let mut l_min: uint = if k < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
                0 as libc::c_int as libc::c_uint
            } else {
                k.wrapping_add(1 as libc::c_int as libc::c_uint)
                    .wrapping_sub((48 as libc::c_int / 8 as libc::c_int) as libc::c_uint)
            };
            i = l_min;
            while i <= k && i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
                let mut l_product: uint128_t =
                    (*p_left.offset(i as isize) as uint128_t).wrapping_mul(*p_right.offset(k.wrapping_sub(i) as isize) as u128);
                r01 = (r01 as u128).wrapping_add(l_product) as uint128_t as uint128_t;
                r2 = (r2 as libc::c_ulong).wrapping_add((r01 < l_product) as libc::c_int as libc::c_ulong) as uint64_t as uint64_t;
                i = i.wrapping_add(1)
            }
            *p_result.offset(k as isize) = r01 as uint64_t;
            r01 = r01 >> 64 as libc::c_int | (r2 as uint128_t) << 64 as libc::c_int;
            r2 = 0 as libc::c_int as uint64_t;
            k = k.wrapping_add(1)
        }
        *p_result.offset((48 as libc::c_int / 8 as libc::c_int * 2 as libc::c_int - 1 as libc::c_int) as isize) = r01 as uint64_t;
    }
    /* Computes p_result = p_left^2. */

    unsafe fn vli_square(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t) {
        let mut r01: uint128_t = 0 as libc::c_int as uint128_t;
        let mut r2: uint64_t = 0 as libc::c_int as uint64_t;
        let mut i: uint = 0;
        let mut k: uint = 0;
        k = 0 as libc::c_int as uint;
        while k < (48 as libc::c_int / 8 as libc::c_int * 2 as libc::c_int - 1 as libc::c_int) as libc::c_uint {
            let mut l_min: uint = if k < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
                0 as libc::c_int as libc::c_uint
            } else {
                k.wrapping_add(1 as libc::c_int as libc::c_uint)
                    .wrapping_sub((48 as libc::c_int / 8 as libc::c_int) as libc::c_uint)
            };
            i = l_min;
            while i <= k && i <= k.wrapping_sub(i) {
                let mut l_product: uint128_t =
                    (*p_left.offset(i as isize) as uint128_t).wrapping_mul(*p_left.offset(k.wrapping_sub(i) as isize) as u128);
                if i < k.wrapping_sub(i) {
                    r2 = (r2 as u128).wrapping_add(l_product >> 127 as libc::c_int) as uint64_t as uint64_t;
                    l_product = (l_product as u128).wrapping_mul(2 as libc::c_int as u128) as uint128_t as uint128_t
                }
                r01 = (r01 as u128).wrapping_add(l_product) as uint128_t as uint128_t;
                r2 = (r2 as libc::c_ulong).wrapping_add((r01 < l_product) as libc::c_int as libc::c_ulong) as uint64_t as uint64_t;
                i = i.wrapping_add(1)
            }
            *p_result.offset(k as isize) = r01 as uint64_t;
            r01 = r01 >> 64 as libc::c_int | (r2 as uint128_t) << 64 as libc::c_int;
            r2 = 0 as libc::c_int as uint64_t;
            k = k.wrapping_add(1)
        }
        *p_result.offset((48 as libc::c_int / 8 as libc::c_int * 2 as libc::c_int - 1 as libc::c_int) as isize) = r01 as uint64_t;
    }
    /* #if SUPPORTS_INT128 */
    /* SUPPORTS_INT128 */
    /* Computes p_result = (p_left + p_right) % p_mod.
    Assumes that p_left < p_mod and p_right < p_mod, p_result != p_mod. */

    unsafe fn vli_modAdd(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t, mut p_mod: *mut uint64_t) {
        let mut l_carry: uint64_t = vli_add(p_result, p_left, p_right);
        if l_carry != 0 || vli_cmp(p_result, p_mod) >= 0 as libc::c_int {
            /* p_result > p_mod (p_result = p_mod + remainder), so subtract p_mod to get remainder. */
            vli_sub(p_result, p_result, p_mod);
        };
    }
    /* Computes p_result = (p_left - p_right) % p_mod.
    Assumes that p_left < p_mod and p_right < p_mod, p_result != p_mod. */

    unsafe fn vli_modSub(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t, mut p_mod: *mut uint64_t) {
        let mut l_borrow: uint64_t = vli_sub(p_result, p_left, p_right);
        if l_borrow != 0 {
            /* In this case, p_result == -diff == (max int) - diff.
            Since -x % d == d - x, we can get the correct result from p_result + p_mod (with overflow). */
            vli_add(p_result, p_result, p_mod);
        };
    }
    //#elif ECC_CURVE == secp384r1

    unsafe fn omega_mult(mut p_result: *mut uint64_t, mut p_right: *mut uint64_t) {
        let mut l_tmp: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_carry: uint64_t = 0;
        let mut l_diff: uint64_t = 0;
        /* Multiply by (2^128 + 2^96 - 2^32 + 1). */
        vli_set(p_result, p_right); /* 1 */
        l_carry = vli_lshift(l_tmp.as_mut_ptr(), p_right, 32 as libc::c_int as uint); /* 2^96 + 1 */
        *p_result.offset((1 as libc::c_int + 48 as libc::c_int / 8 as libc::c_int) as isize) = l_carry.wrapping_add(vli_add(
            p_result.offset(1 as libc::c_int as isize),
            p_result.offset(1 as libc::c_int as isize),
            l_tmp.as_mut_ptr(),
        )); /* 2^128 + 2^96 + 1 */
        *p_result.offset((2 as libc::c_int + 48 as libc::c_int / 8 as libc::c_int) as isize) = vli_add(
            p_result.offset(2 as libc::c_int as isize),
            p_result.offset(2 as libc::c_int as isize),
            p_right,
        ); /* 2^128 + 2^96 - 2^32 + 1 */
        l_carry = (l_carry as libc::c_ulong).wrapping_add(vli_sub(p_result, p_result, l_tmp.as_mut_ptr())) as uint64_t as uint64_t;
        l_diff = (*p_result.offset((48 as libc::c_int / 8 as libc::c_int) as isize)).wrapping_sub(l_carry);
        if l_diff > *p_result.offset((48 as libc::c_int / 8 as libc::c_int) as isize) {
            /* Propagate borrow if necessary. */
            let mut i: uint = 0;
            i = (1 as libc::c_int + 48 as libc::c_int / 8 as libc::c_int) as uint;
            loop {
                let ref mut fresh1 = *p_result.offset(i as isize);
                *fresh1 = (*fresh1).wrapping_sub(1);
                if *p_result.offset(i as isize) != -(1 as libc::c_int) as uint64_t {
                    break;
                }
                i = i.wrapping_add(1)
            }
        }
        *p_result.offset((48 as libc::c_int / 8 as libc::c_int) as isize) = l_diff;
    }
    /* Computes p_result = p_product % curve_p
    see PDF "Comparing Elliptic Curve Cryptography and RSA on 8-bit CPUs"
    section "Curve-Specific Optimizations" */

    unsafe fn vli_mmod_fast(mut p_result: *mut uint64_t, mut p_product: *mut uint64_t) {
        let mut l_tmp: [uint64_t; 12] = std::mem::MaybeUninit::uninit().assume_init();
        while vli_isZero(p_product.offset((48 as libc::c_int / 8 as libc::c_int) as isize)) == 0 {
            /* While c1 != 0 */
            let mut l_carry: uint64_t = 0 as libc::c_int as uint64_t; /* tmp = w * c1 */
            let mut i: uint = 0; /* p = c0 */
            vli_clear(l_tmp.as_mut_ptr());
            vli_clear(l_tmp.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize));
            omega_mult(l_tmp.as_mut_ptr(), p_product.offset((48 as libc::c_int / 8 as libc::c_int) as isize));
            vli_clear(p_product.offset((48 as libc::c_int / 8 as libc::c_int) as isize));
            /* (c1, c0) = c0 + w * c1 */
            i = 0 as libc::c_int as uint;
            while i < (48 as libc::c_int / 8 as libc::c_int + 3 as libc::c_int) as libc::c_uint {
                let mut l_sum: uint64_t = (*p_product.offset(i as isize)).wrapping_add(l_tmp[i as usize]).wrapping_add(l_carry);
                if l_sum != *p_product.offset(i as isize) {
                    l_carry = (l_sum < *p_product.offset(i as isize)) as libc::c_int as uint64_t
                }
                *p_product.offset(i as isize) = l_sum;
                i = i.wrapping_add(1)
            }
        }
        while vli_cmp(p_product, curve_p.as_mut_ptr()) > 0 as libc::c_int {
            vli_sub(p_product, p_product, curve_p.as_mut_ptr());
        }
        vli_set(p_result, p_product);
    }
    //#endif
    /* Computes p_result = (p_left * p_right) % curve_p. */

    unsafe fn vli_modMult_fast(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t) {
        let mut l_product: [uint64_t; 12] = std::mem::MaybeUninit::uninit().assume_init();
        vli_mult(l_product.as_mut_ptr(), p_left, p_right);
        vli_mmod_fast(p_result, l_product.as_mut_ptr());
    }
    /* Computes p_result = p_left^2 % curve_p. */

    unsafe fn vli_modSquare_fast(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t) {
        let mut l_product: [uint64_t; 12] = std::mem::MaybeUninit::uninit().assume_init();
        vli_square(l_product.as_mut_ptr(), p_left);
        vli_mmod_fast(p_result, l_product.as_mut_ptr());
    }
    /* Computes p_result = (1 / p_input) % p_mod. All VLIs are the same size.
    See "From Euclid's GCD to Montgomery Multiplication to the Great Divide"
    https://labs.oracle.com/techrep/2001/smli_tr-2001-95.pdf */

    unsafe fn vli_modInv(mut p_result: *mut uint64_t, mut p_input: *mut uint64_t, mut p_mod: *mut uint64_t) {
        let mut a: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut b: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut u: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut v: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_carry: uint64_t = 0;
        let mut l_cmpResult: libc::c_int = 0;
        if vli_isZero(p_input) != 0 {
            vli_clear(p_result);
            return;
        }
        vli_set(a.as_mut_ptr(), p_input);
        vli_set(b.as_mut_ptr(), p_mod);
        vli_clear(u.as_mut_ptr());
        u[0 as libc::c_int as usize] = 1 as libc::c_int as uint64_t;
        vli_clear(v.as_mut_ptr());
        loop {
            l_cmpResult = vli_cmp(a.as_mut_ptr(), b.as_mut_ptr());
            if !(l_cmpResult != 0 as libc::c_int) {
                break;
            }
            l_carry = 0 as libc::c_int as uint64_t;
            if a[0 as libc::c_int as usize] & 1 as libc::c_int as libc::c_ulong == 0 {
                vli_rshift1(a.as_mut_ptr());
                if u[0 as libc::c_int as usize] & 1 as libc::c_int as libc::c_ulong != 0 {
                    l_carry = vli_add(u.as_mut_ptr(), u.as_mut_ptr(), p_mod)
                }
                vli_rshift1(u.as_mut_ptr());
                if l_carry != 0 {
                    u[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] =
                        (u[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] as libc::c_ulonglong
                            | 0x8000000000000000 as libc::c_ulonglong) as uint64_t
                }
            } else if b[0 as libc::c_int as usize] & 1 as libc::c_int as libc::c_ulong == 0 {
                vli_rshift1(b.as_mut_ptr());
                if v[0 as libc::c_int as usize] & 1 as libc::c_int as libc::c_ulong != 0 {
                    l_carry = vli_add(v.as_mut_ptr(), v.as_mut_ptr(), p_mod)
                }
                vli_rshift1(v.as_mut_ptr());
                if l_carry != 0 {
                    v[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] =
                        (v[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] as libc::c_ulonglong
                            | 0x8000000000000000 as libc::c_ulonglong) as uint64_t
                }
            } else if l_cmpResult > 0 as libc::c_int {
                vli_sub(a.as_mut_ptr(), a.as_mut_ptr(), b.as_mut_ptr());
                vli_rshift1(a.as_mut_ptr());
                if vli_cmp(u.as_mut_ptr(), v.as_mut_ptr()) < 0 as libc::c_int {
                    vli_add(u.as_mut_ptr(), u.as_mut_ptr(), p_mod);
                }
                vli_sub(u.as_mut_ptr(), u.as_mut_ptr(), v.as_mut_ptr());
                if u[0 as libc::c_int as usize] & 1 as libc::c_int as libc::c_ulong != 0 {
                    l_carry = vli_add(u.as_mut_ptr(), u.as_mut_ptr(), p_mod)
                }
                vli_rshift1(u.as_mut_ptr());
                if l_carry != 0 {
                    u[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] =
                        (u[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] as libc::c_ulonglong
                            | 0x8000000000000000 as libc::c_ulonglong) as uint64_t
                }
            } else {
                vli_sub(b.as_mut_ptr(), b.as_mut_ptr(), a.as_mut_ptr());
                vli_rshift1(b.as_mut_ptr());
                if vli_cmp(v.as_mut_ptr(), u.as_mut_ptr()) < 0 as libc::c_int {
                    vli_add(v.as_mut_ptr(), v.as_mut_ptr(), p_mod);
                }
                vli_sub(v.as_mut_ptr(), v.as_mut_ptr(), u.as_mut_ptr());
                if v[0 as libc::c_int as usize] & 1 as libc::c_int as libc::c_ulong != 0 {
                    l_carry = vli_add(v.as_mut_ptr(), v.as_mut_ptr(), p_mod)
                }
                vli_rshift1(v.as_mut_ptr());
                if l_carry != 0 {
                    v[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] =
                        (v[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] as libc::c_ulonglong
                            | 0x8000000000000000 as libc::c_ulonglong) as uint64_t
                }
            }
        }
        vli_set(p_result, u.as_mut_ptr());
    }
    /* ------ Point operations ------ */
    /* Returns 1 if p_point is the point at infinity, 0 otherwise. */

    unsafe fn EccPoint_isZero(mut p_point: *mut EccPoint) -> libc::c_int {
        return (vli_isZero((*p_point).x.as_mut_ptr()) != 0 && vli_isZero((*p_point).y.as_mut_ptr()) != 0) as libc::c_int;
    }
    /* Point multiplication algorithm using Montgomery's ladder with co-Z coordinates.
    From http://eprint.iacr.org/2011/338.pdf
    */
    /* Double in place */

    unsafe fn EccPoint_double_jacobian(mut X1: *mut uint64_t, mut Y1: *mut uint64_t, mut Z1: *mut uint64_t) {
        /* t1 = X, t2 = Y, t3 = Z */
        let mut t4: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* t4 = y1^2 */
        let mut t5: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* t5 = x1*y1^2 = A */
        if vli_isZero(Z1) != 0 {
            return;
        } /* t4 = y1^4 */
        vli_modSquare_fast(t4.as_mut_ptr(), Y1); /* t2 = y1*z1 = z3 */
        vli_modMult_fast(t5.as_mut_ptr(), X1, t4.as_mut_ptr()); /* t3 = z1^2 */
        vli_modSquare_fast(t4.as_mut_ptr(), t4.as_mut_ptr()); /* t1 = x1 + z1^2 */
        vli_modMult_fast(Y1, Y1, Z1); /* t3 = 2*z1^2 */
        vli_modSquare_fast(Z1, Z1); /* t3 = x1 - z1^2 */
        vli_modAdd(X1, X1, Z1, curve_p.as_mut_ptr()); /* t1 = x1^2 - z1^4 */
        vli_modAdd(Z1, Z1, Z1, curve_p.as_mut_ptr()); /* t3 = 2*(x1^2 - z1^4) */
        vli_modSub(Z1, X1, Z1, curve_p.as_mut_ptr()); /* t1 = 3*(x1^2 - z1^4) */
        vli_modMult_fast(X1, X1, Z1);
        vli_modAdd(Z1, X1, X1, curve_p.as_mut_ptr());
        vli_modAdd(X1, X1, Z1, curve_p.as_mut_ptr());
        if vli_testBit(X1, 0 as libc::c_int as uint) != 0 {
            let mut l_carry: uint64_t = vli_add(X1, X1, curve_p.as_mut_ptr());
            vli_rshift1(X1);
            let ref mut fresh2 = *X1.offset((48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as isize);
            *fresh2 |= l_carry << 63 as libc::c_int
        } else {
            vli_rshift1(X1);
        }
        /* t1 = 3/2*(x1^2 - z1^4) = B */
        vli_modSquare_fast(Z1, X1); /* t3 = B^2 */
        vli_modSub(Z1, Z1, t5.as_mut_ptr(), curve_p.as_mut_ptr()); /* t3 = B^2 - A */
        vli_modSub(Z1, Z1, t5.as_mut_ptr(), curve_p.as_mut_ptr()); /* t3 = B^2 - 2A = x3 */
        vli_modSub(t5.as_mut_ptr(), t5.as_mut_ptr(), Z1, curve_p.as_mut_ptr()); /* t5 = A - x3 */
        vli_modMult_fast(X1, X1, t5.as_mut_ptr()); /* t1 = B * (A - x3) */
        vli_modSub(t4.as_mut_ptr(), X1, t4.as_mut_ptr(), curve_p.as_mut_ptr()); /* t4 = B * (A - x3) - y1^4 = y3 */
        vli_set(X1, Z1);
        vli_set(Z1, Y1);
        vli_set(Y1, t4.as_mut_ptr());
    }
    /* Modify (x1, y1) => (x1 * z^2, y1 * z^3) */

    unsafe fn apply_z(mut X1: *mut uint64_t, mut Y1: *mut uint64_t, mut Z: *mut uint64_t) {
        let mut t1: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* z^2 */
        vli_modSquare_fast(t1.as_mut_ptr(), Z); /* x1 * z^2 */
        vli_modMult_fast(X1, X1, t1.as_mut_ptr()); /* z^3 */
        vli_modMult_fast(t1.as_mut_ptr(), t1.as_mut_ptr(), Z);
        vli_modMult_fast(Y1, Y1, t1.as_mut_ptr());
        /* y1 * z^3 */
    }
    /* P = (x1, y1) => 2P, (x2, y2) => P' */

    unsafe fn XYcZ_initial_double(
        mut X1: *mut uint64_t,
        mut Y1: *mut uint64_t,
        mut X2: *mut uint64_t,
        mut Y2: *mut uint64_t,
        mut p_initialZ: *mut uint64_t,
    ) {
        let mut z: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        vli_set(X2, X1);
        vli_set(Y2, Y1);
        vli_clear(z.as_mut_ptr());
        z[0 as libc::c_int as usize] = 1 as libc::c_int as uint64_t;
        if !p_initialZ.is_null() {
            vli_set(z.as_mut_ptr(), p_initialZ);
        }
        apply_z(X1, Y1, z.as_mut_ptr());
        EccPoint_double_jacobian(X1, Y1, z.as_mut_ptr());
        apply_z(X2, Y2, z.as_mut_ptr());
    }
    /* Input P = (x1, y1, Z), Q = (x2, y2, Z)
    Output P' = (x1', y1', Z3), P + Q = (x3, y3, Z3)
    or P => P', Q => P + Q
    */

    unsafe fn XYcZ_add(mut X1: *mut uint64_t, mut Y1: *mut uint64_t, mut X2: *mut uint64_t, mut Y2: *mut uint64_t) {
        /* t1 = X1, t2 = Y1, t3 = X2, t4 = Y2 */
        let mut t5: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* t5 = x2 - x1 */
        vli_modSub(t5.as_mut_ptr(), X2, X1, curve_p.as_mut_ptr()); /* t5 = (x2 - x1)^2 = A */
        vli_modSquare_fast(t5.as_mut_ptr(), t5.as_mut_ptr()); /* t1 = x1*A = B */
        vli_modMult_fast(X1, X1, t5.as_mut_ptr()); /* t3 = x2*A = C */
        vli_modMult_fast(X2, X2, t5.as_mut_ptr()); /* t4 = y2 - y1 */
        vli_modSub(Y2, Y2, Y1, curve_p.as_mut_ptr()); /* t5 = (y2 - y1)^2 = D */
        vli_modSquare_fast(t5.as_mut_ptr(), Y2); /* t5 = D - B */
        vli_modSub(t5.as_mut_ptr(), t5.as_mut_ptr(), X1, curve_p.as_mut_ptr()); /* t5 = D - B - C = x3 */
        vli_modSub(t5.as_mut_ptr(), t5.as_mut_ptr(), X2, curve_p.as_mut_ptr()); /* t3 = C - B */
        vli_modSub(X2, X2, X1, curve_p.as_mut_ptr()); /* t2 = y1*(C - B) */
        vli_modMult_fast(Y1, Y1, X2); /* t3 = B - x3 */
        vli_modSub(X2, X1, t5.as_mut_ptr(), curve_p.as_mut_ptr()); /* t4 = (y2 - y1)*(B - x3) */
        vli_modMult_fast(Y2, Y2, X2); /* t4 = y3 */
        vli_modSub(Y2, Y2, Y1, curve_p.as_mut_ptr());
        vli_set(X2, t5.as_mut_ptr());
    }
    /* Input P = (x1, y1, Z), Q = (x2, y2, Z)
    Output P + Q = (x3, y3, Z3), P - Q = (x3', y3', Z3)
    or P => P - Q, Q => P + Q
    */

    unsafe fn XYcZ_addC(mut X1: *mut uint64_t, mut Y1: *mut uint64_t, mut X2: *mut uint64_t, mut Y2: *mut uint64_t) {
        /* t1 = X1, t2 = Y1, t3 = X2, t4 = Y2 */
        let mut t5: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* t5 = x2 - x1 */
        let mut t6: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* t5 = (x2 - x1)^2 = A */
        let mut t7: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init(); /* t1 = x1*A = B */
        vli_modSub(t5.as_mut_ptr(), X2, X1, curve_p.as_mut_ptr()); /* t3 = x2*A = C */
        vli_modSquare_fast(t5.as_mut_ptr(), t5.as_mut_ptr()); /* t4 = y2 + y1 */
        vli_modMult_fast(X1, X1, t5.as_mut_ptr()); /* t4 = y2 - y1 */
        vli_modMult_fast(X2, X2, t5.as_mut_ptr()); /* t6 = C - B */
        vli_modAdd(t5.as_mut_ptr(), Y2, Y1, curve_p.as_mut_ptr()); /* t2 = y1 * (C - B) */
        vli_modSub(Y2, Y2, Y1, curve_p.as_mut_ptr()); /* t6 = B + C */
        vli_modSub(t6.as_mut_ptr(), X2, X1, curve_p.as_mut_ptr()); /* t3 = (y2 - y1)^2 */
        vli_modMult_fast(Y1, Y1, t6.as_mut_ptr()); /* t3 = x3 */
        vli_modAdd(t6.as_mut_ptr(), X1, X2, curve_p.as_mut_ptr()); /* t7 = B - x3 */
        vli_modSquare_fast(X2, Y2); /* t4 = (y2 - y1)*(B - x3) */
        vli_modSub(X2, X2, t6.as_mut_ptr(), curve_p.as_mut_ptr()); /* t4 = y3 */
        vli_modSub(t7.as_mut_ptr(), X1, X2, curve_p.as_mut_ptr()); /* t7 = (y2 + y1)^2 = F */
        vli_modMult_fast(Y2, Y2, t7.as_mut_ptr()); /* t7 = x3' */
        vli_modSub(Y2, Y2, Y1, curve_p.as_mut_ptr()); /* t6 = x3' - B */
        vli_modSquare_fast(t7.as_mut_ptr(), t5.as_mut_ptr()); /* t6 = (y2 + y1)*(x3' - B) */
        vli_modSub(t7.as_mut_ptr(), t7.as_mut_ptr(), t6.as_mut_ptr(), curve_p.as_mut_ptr()); /* t2 = y3' */
        vli_modSub(t6.as_mut_ptr(), t7.as_mut_ptr(), X1, curve_p.as_mut_ptr());
        vli_modMult_fast(t6.as_mut_ptr(), t6.as_mut_ptr(), t5.as_mut_ptr());
        vli_modSub(Y1, t6.as_mut_ptr(), Y1, curve_p.as_mut_ptr());
        vli_set(X1, t7.as_mut_ptr());
    }

    unsafe fn EccPoint_mult(mut p_result: *mut EccPoint, mut p_point: *mut EccPoint, mut p_scalar: *mut uint64_t, mut p_initialZ: *mut uint64_t) {
        /* R0 and R1 */
        let mut Rx: [[uint64_t; 6]; 2] = std::mem::MaybeUninit::uninit().assume_init();
        let mut Ry: [[uint64_t; 6]; 2] = std::mem::MaybeUninit::uninit().assume_init();
        let mut z: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut i: libc::c_int = 0;
        let mut nb: libc::c_int = 0;
        vli_set(Rx[1 as libc::c_int as usize].as_mut_ptr(), (*p_point).x.as_mut_ptr());
        vli_set(Ry[1 as libc::c_int as usize].as_mut_ptr(), (*p_point).y.as_mut_ptr());
        XYcZ_initial_double(
            Rx[1 as libc::c_int as usize].as_mut_ptr(),
            Ry[1 as libc::c_int as usize].as_mut_ptr(),
            Rx[0 as libc::c_int as usize].as_mut_ptr(),
            Ry[0 as libc::c_int as usize].as_mut_ptr(),
            p_initialZ,
        );
        i = vli_numBits(p_scalar).wrapping_sub(2 as libc::c_int as libc::c_uint) as libc::c_int;
        while i > 0 as libc::c_int {
            nb = (vli_testBit(p_scalar, i as uint) == 0) as libc::c_int;
            XYcZ_addC(
                Rx[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
                Ry[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
                Rx[nb as usize].as_mut_ptr(),
                Ry[nb as usize].as_mut_ptr(),
            );
            XYcZ_add(
                Rx[nb as usize].as_mut_ptr(),
                Ry[nb as usize].as_mut_ptr(),
                Rx[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
                Ry[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
            );
            i -= 1
        }
        nb = (vli_testBit(p_scalar, 0 as libc::c_int as uint) == 0) as libc::c_int;
        XYcZ_addC(
            Rx[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
            Ry[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
            Rx[nb as usize].as_mut_ptr(),
            Ry[nb as usize].as_mut_ptr(),
        );
        /* Find final 1/Z value. */
        vli_modSub(
            z.as_mut_ptr(),
            Rx[1 as libc::c_int as usize].as_mut_ptr(),
            Rx[0 as libc::c_int as usize].as_mut_ptr(),
            curve_p.as_mut_ptr(),
        ); /* X1 - X0 */
        vli_modMult_fast(z.as_mut_ptr(), z.as_mut_ptr(), Ry[(1 as libc::c_int - nb) as usize].as_mut_ptr()); /* Yb * (X1 - X0) */
        vli_modMult_fast(z.as_mut_ptr(), z.as_mut_ptr(), (*p_point).x.as_mut_ptr()); /* xP * Yb * (X1 - X0) */
        vli_modInv(z.as_mut_ptr(), z.as_mut_ptr(), curve_p.as_mut_ptr()); /* 1 / (xP * Yb * (X1 - X0)) */
        vli_modMult_fast(z.as_mut_ptr(), z.as_mut_ptr(), (*p_point).y.as_mut_ptr()); /* yP / (xP * Yb * (X1 - X0)) */
        vli_modMult_fast(z.as_mut_ptr(), z.as_mut_ptr(), Rx[(1 as libc::c_int - nb) as usize].as_mut_ptr()); /* Xb * yP / (xP * Yb * (X1 - X0)) */
        /* End 1/Z calculation */
        XYcZ_add(
            Rx[nb as usize].as_mut_ptr(),
            Ry[nb as usize].as_mut_ptr(),
            Rx[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
            Ry[(1 as libc::c_int - nb) as usize].as_mut_ptr(),
        );
        apply_z(
            Rx[0 as libc::c_int as usize].as_mut_ptr(),
            Ry[0 as libc::c_int as usize].as_mut_ptr(),
            z.as_mut_ptr(),
        );
        vli_set((*p_result).x.as_mut_ptr(), Rx[0 as libc::c_int as usize].as_mut_ptr());
        vli_set((*p_result).y.as_mut_ptr(), Ry[0 as libc::c_int as usize].as_mut_ptr());
    }

    unsafe fn ecc_bytes2native(mut p_native: *mut uint64_t, mut p_bytes: *const uint8_t) {
        let mut i: libc::c_uint = 0;
        i = 0 as libc::c_int as libc::c_uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            let mut p_digit: *const uint8_t = p_bytes.offset(
                (8 as libc::c_int as libc::c_uint)
                    .wrapping_mul(((48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as libc::c_uint).wrapping_sub(i))
                    as isize,
            );
            *p_native.offset(i as isize) = (*p_digit.offset(0 as libc::c_int as isize) as uint64_t) << 56 as libc::c_int
                | (*p_digit.offset(1 as libc::c_int as isize) as uint64_t) << 48 as libc::c_int
                | (*p_digit.offset(2 as libc::c_int as isize) as uint64_t) << 40 as libc::c_int
                | (*p_digit.offset(3 as libc::c_int as isize) as uint64_t) << 32 as libc::c_int
                | (*p_digit.offset(4 as libc::c_int as isize) as uint64_t) << 24 as libc::c_int
                | (*p_digit.offset(5 as libc::c_int as isize) as uint64_t) << 16 as libc::c_int
                | (*p_digit.offset(6 as libc::c_int as isize) as uint64_t) << 8 as libc::c_int
                | *p_digit.offset(7 as libc::c_int as isize) as uint64_t;
            i = i.wrapping_add(1)
        }
    }

    unsafe fn ecc_native2bytes(mut p_bytes: *mut uint8_t, mut p_native: *const uint64_t) {
        let mut i: libc::c_uint = 0;
        i = 0 as libc::c_int as libc::c_uint;
        while i < (48 as libc::c_int / 8 as libc::c_int) as libc::c_uint {
            let mut p_digit: *mut uint8_t = p_bytes.offset(
                (8 as libc::c_int as libc::c_uint)
                    .wrapping_mul(((48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as libc::c_uint).wrapping_sub(i))
                    as isize,
            );
            *p_digit.offset(0 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 56 as libc::c_int) as uint8_t;
            *p_digit.offset(1 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 48 as libc::c_int) as uint8_t;
            *p_digit.offset(2 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 40 as libc::c_int) as uint8_t;
            *p_digit.offset(3 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 32 as libc::c_int) as uint8_t;
            *p_digit.offset(4 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 24 as libc::c_int) as uint8_t;
            *p_digit.offset(5 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 16 as libc::c_int) as uint8_t;
            *p_digit.offset(6 as libc::c_int as isize) = (*p_native.offset(i as isize) >> 8 as libc::c_int) as uint8_t;
            *p_digit.offset(7 as libc::c_int as isize) = *p_native.offset(i as isize) as uint8_t;
            i = i.wrapping_add(1)
        }
    }
    /* Compute a = sqrt(a) (mod curve_p). */

    unsafe fn mod_sqrt(mut a: *mut uint64_t) {
        let mut i: libc::c_uint = 0;
        let mut p1: [uint64_t; 6] = [1 as libc::c_int as uint64_t, 0, 0, 0, 0, 0];
        let mut l_result: [uint64_t; 6] = [1 as libc::c_int as uint64_t, 0, 0, 0, 0, 0];
        /* Since curve_p == 3 (mod 4) for all supported curves, we can
        compute sqrt(a) = a^((curve_p + 1) / 4) (mod curve_p). */
        vli_add(p1.as_mut_ptr(), curve_p.as_mut_ptr(), p1.as_mut_ptr()); /* p1 = curve_p + 1 */
        i = vli_numBits(p1.as_mut_ptr()).wrapping_sub(1 as libc::c_int as libc::c_uint); /* -a = 3 */
        while i > 1 as libc::c_int as libc::c_uint {
            vli_modSquare_fast(l_result.as_mut_ptr(), l_result.as_mut_ptr()); /* y = x^2 */
            if vli_testBit(p1.as_mut_ptr(), i) != 0 {
                vli_modMult_fast(l_result.as_mut_ptr(), l_result.as_mut_ptr(), a);
                /* y = x^2 - 3 */
            } /* y = x^3 - 3x */
            i = i.wrapping_sub(1)
        } /* y = x^3 - 3x + b */
        vli_set(a, l_result.as_mut_ptr());
    }

    unsafe fn ecc_point_decompress(mut p_point: *mut EccPoint, mut p_compressed: *const uint8_t) {
        let mut _3: [uint64_t; 6] = [3 as libc::c_int as uint64_t, 0, 0, 0, 0, 0];
        ecc_bytes2native((*p_point).x.as_mut_ptr(), p_compressed.offset(1 as libc::c_int as isize));
        vli_modSquare_fast((*p_point).y.as_mut_ptr(), (*p_point).x.as_mut_ptr());
        vli_modSub(
            (*p_point).y.as_mut_ptr(),
            (*p_point).y.as_mut_ptr(),
            _3.as_mut_ptr(),
            curve_p.as_mut_ptr(),
        );
        vli_modMult_fast((*p_point).y.as_mut_ptr(), (*p_point).y.as_mut_ptr(), (*p_point).x.as_mut_ptr());
        vli_modAdd(
            (*p_point).y.as_mut_ptr(),
            (*p_point).y.as_mut_ptr(),
            curve_b.as_mut_ptr(),
            curve_p.as_mut_ptr(),
        );
        mod_sqrt((*p_point).y.as_mut_ptr());
        if (*p_point).y[0 as libc::c_int as usize] & 0x1 as libc::c_int as libc::c_ulong
            != (*p_compressed.offset(0 as libc::c_int as isize) as libc::c_int & 0x1 as libc::c_int) as libc::c_ulong
        {
            vli_sub((*p_point).y.as_mut_ptr(), curve_p.as_mut_ptr(), (*p_point).y.as_mut_ptr());
        };
    }
    pub unsafe fn ecc_make_key(mut p_publicKey: *mut uint8_t, mut p_privateKey: *mut uint8_t) -> libc::c_int {
        let mut l_private: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_public: EccPoint = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_tries: libc::c_uint = 0 as libc::c_int as libc::c_uint;
        loop {
            if getRandomNumber(l_private.as_mut_ptr()) == 0 || {
                let fresh3 = l_tries;
                l_tries = l_tries.wrapping_add(1);
                (fresh3) >= 1024 as libc::c_int as libc::c_uint
            } {
                return 0 as libc::c_int;
            }
            if !(vli_isZero(l_private.as_mut_ptr()) != 0) {
                /* Make sure the private key is in the range [1, n-1].
                For the supported curves, n is always large enough that we only need to subtract once at most. */
                if vli_cmp(curve_n.as_mut_ptr(), l_private.as_mut_ptr()) != 1 as libc::c_int {
                    vli_sub(l_private.as_mut_ptr(), l_private.as_mut_ptr(), curve_n.as_mut_ptr());
                }
                EccPoint_mult(&mut l_public, &mut curve_G, l_private.as_mut_ptr(), 0 as *mut uint64_t);
            }
            if !(EccPoint_isZero(&mut l_public) != 0) {
                break;
            }
        }
        ecc_native2bytes(p_privateKey, l_private.as_mut_ptr() as *const uint64_t);
        ecc_native2bytes(p_publicKey.offset(1 as libc::c_int as isize), l_public.x.as_mut_ptr() as *const uint64_t);
        *p_publicKey.offset(0 as libc::c_int as isize) =
            (2 as libc::c_int as libc::c_ulong).wrapping_add(l_public.y[0 as libc::c_int as usize] & 0x1 as libc::c_int as libc::c_ulong) as uint8_t;
        return 1 as libc::c_int;
    }
    pub unsafe fn ecdh_shared_secret(mut p_publicKey: *const uint8_t, mut p_privateKey: *const uint8_t, mut p_secret: *mut uint8_t) -> libc::c_int {
        let mut l_public: EccPoint = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_private: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_random: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        if getRandomNumber(l_random.as_mut_ptr()) == 0 {
            return 0 as libc::c_int;
        }
        ecc_point_decompress(&mut l_public, p_publicKey);
        ecc_bytes2native(l_private.as_mut_ptr(), p_privateKey);
        let mut l_product: EccPoint = EccPoint { x: [0; 6], y: [0; 6] };
        EccPoint_mult(&mut l_product, &mut l_public, l_private.as_mut_ptr(), l_random.as_mut_ptr());
        ecc_native2bytes(p_secret, l_product.x.as_mut_ptr() as *const uint64_t);
        return (EccPoint_isZero(&mut l_product) == 0) as libc::c_int;
    }
    /* -------- ECDSA code -------- */
    /* Computes p_result = (p_left * p_right) % p_mod. */

    unsafe fn vli_modMult(mut p_result: *mut uint64_t, mut p_left: *mut uint64_t, mut p_right: *mut uint64_t, mut p_mod: *mut uint64_t) {
        let mut l_product: [uint64_t; 12] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_modMultiple: [uint64_t; 12] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_digitShift: uint = 0;
        let mut l_bitShift: uint = 0;
        let mut l_productBits: uint = 0;
        let mut l_modBits: uint = vli_numBits(p_mod);
        vli_mult(l_product.as_mut_ptr(), p_left, p_right);
        l_productBits = vli_numBits(l_product.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize));
        if l_productBits != 0 {
            l_productBits = (l_productBits as libc::c_uint).wrapping_add((48 as libc::c_int / 8 as libc::c_int * 64 as libc::c_int) as libc::c_uint)
                as uint as uint
        } else {
            l_productBits = vli_numBits(l_product.as_mut_ptr())
        }
        if l_productBits < l_modBits {
            /* l_product < p_mod. */
            vli_set(p_result, l_product.as_mut_ptr());
            return;
        }
        /* Shift p_mod by (l_leftBits - l_modBits). This multiplies p_mod by the largest
        power of two possible while still resulting in a number less than p_left. */
        vli_clear(l_modMultiple.as_mut_ptr());
        vli_clear(l_modMultiple.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize));
        l_digitShift = l_productBits.wrapping_sub(l_modBits).wrapping_div(64 as libc::c_int as libc::c_uint);
        l_bitShift = l_productBits.wrapping_sub(l_modBits).wrapping_rem(64 as libc::c_int as libc::c_uint);
        if l_bitShift != 0 {
            l_modMultiple[l_digitShift.wrapping_add((48 as libc::c_int / 8 as libc::c_int) as libc::c_uint) as usize] =
                vli_lshift(l_modMultiple.as_mut_ptr().offset(l_digitShift as isize), p_mod, l_bitShift)
        } else {
            vli_set(l_modMultiple.as_mut_ptr().offset(l_digitShift as isize), p_mod);
        }
        /* Subtract all multiples of p_mod to get the remainder. */
        vli_clear(p_result); /* Use p_result as a temp var to store 1 (for subtraction) */
        *p_result.offset(0 as libc::c_int as isize) = 1 as libc::c_int as uint64_t;
        while l_productBits > (48 as libc::c_int / 8 as libc::c_int * 64 as libc::c_int) as libc::c_uint
            || vli_cmp(l_modMultiple.as_mut_ptr(), p_mod) >= 0 as libc::c_int
        {
            let mut l_cmp: libc::c_int = vli_cmp(
                l_modMultiple.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
                l_product.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
            );
            if l_cmp < 0 as libc::c_int
                || l_cmp == 0 as libc::c_int && vli_cmp(l_modMultiple.as_mut_ptr(), l_product.as_mut_ptr()) <= 0 as libc::c_int
            {
                if vli_sub(l_product.as_mut_ptr(), l_product.as_mut_ptr(), l_modMultiple.as_mut_ptr()) != 0 {
                    /* borrow */
                    vli_sub(
                        l_product.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
                        l_product.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
                        p_result,
                    );
                }
                vli_sub(
                    l_product.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
                    l_product.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
                    l_modMultiple.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize),
                );
            }
            let mut l_carry: uint64_t =
                (l_modMultiple[(48 as libc::c_int / 8 as libc::c_int) as usize] & 0x1 as libc::c_int as libc::c_ulong) << 63 as libc::c_int;
            vli_rshift1(l_modMultiple.as_mut_ptr().offset((48 as libc::c_int / 8 as libc::c_int) as isize));
            vli_rshift1(l_modMultiple.as_mut_ptr());
            l_modMultiple[(48 as libc::c_int / 8 as libc::c_int - 1 as libc::c_int) as usize] |= l_carry;
            l_productBits = l_productBits.wrapping_sub(1)
        }
        vli_set(p_result, l_product.as_mut_ptr());
    }

    unsafe fn umax(mut a: uint, mut b: uint) -> uint {
        a.max(b)
    }
    pub unsafe fn ecdsa_sign(mut p_privateKey: *const uint8_t, mut p_hash: *const uint8_t, mut p_signature: *mut uint8_t) -> libc::c_int {
        let mut k: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_tmp: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_s: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut p: EccPoint = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_tries: libc::c_uint = 0 as libc::c_int as libc::c_uint;
        loop {
            if getRandomNumber(k.as_mut_ptr()) == 0 || {
                let fresh4 = l_tries;
                l_tries = l_tries.wrapping_add(1);
                (fresh4) >= 1024 as libc::c_int as libc::c_uint
            } {
                return 0 as libc::c_int;
            }
            if !(vli_isZero(k.as_mut_ptr()) != 0) {
                if vli_cmp(curve_n.as_mut_ptr(), k.as_mut_ptr()) != 1 as libc::c_int {
                    vli_sub(k.as_mut_ptr(), k.as_mut_ptr(), curve_n.as_mut_ptr());
                }
                /* tmp = k * G */
                EccPoint_mult(&mut p, &mut curve_G, k.as_mut_ptr(), 0 as *mut uint64_t);
                /* r = x1 (mod n) */
                if vli_cmp(curve_n.as_mut_ptr(), p.x.as_mut_ptr()) != 1 as libc::c_int {
                    vli_sub(p.x.as_mut_ptr(), p.x.as_mut_ptr(), curve_n.as_mut_ptr());
                    /* s = r*d */
                }
            } /* s = e + r*d */
            if !(vli_isZero(p.x.as_mut_ptr()) != 0) {
                break; /* k = 1 / k */
            }
        } /* s = (e + r*d) / k */
        ecc_native2bytes(p_signature, p.x.as_mut_ptr() as *const uint64_t);
        ecc_bytes2native(l_tmp.as_mut_ptr(), p_privateKey);
        vli_modMult(l_s.as_mut_ptr(), p.x.as_mut_ptr(), l_tmp.as_mut_ptr(), curve_n.as_mut_ptr());
        ecc_bytes2native(l_tmp.as_mut_ptr(), p_hash);
        vli_modAdd(l_s.as_mut_ptr(), l_tmp.as_mut_ptr(), l_s.as_mut_ptr(), curve_n.as_mut_ptr());
        vli_modInv(k.as_mut_ptr(), k.as_mut_ptr(), curve_n.as_mut_ptr());
        vli_modMult(l_s.as_mut_ptr(), l_s.as_mut_ptr(), k.as_mut_ptr(), curve_n.as_mut_ptr());
        ecc_native2bytes(p_signature.offset(48 as libc::c_int as isize), l_s.as_mut_ptr() as *const uint64_t);
        return 1 as libc::c_int;
    }
    pub unsafe fn ecdsa_verify(mut p_publicKey: *const uint8_t, mut p_hash: *const uint8_t, mut p_signature: *const uint8_t) -> libc::c_int {
        let mut u1: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut u2: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut z: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_public: EccPoint = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_sum: EccPoint = std::mem::MaybeUninit::uninit().assume_init();
        let mut rx: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut ry: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut tx: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut ty: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut tz: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_r: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        let mut l_s: [uint64_t; 6] = std::mem::MaybeUninit::uninit().assume_init();
        ecc_point_decompress(&mut l_public, p_publicKey);
        ecc_bytes2native(l_r.as_mut_ptr(), p_signature);
        ecc_bytes2native(l_s.as_mut_ptr(), p_signature.offset(48 as libc::c_int as isize));
        if vli_isZero(l_r.as_mut_ptr()) != 0 || vli_isZero(l_s.as_mut_ptr()) != 0 {
            /* r, s must not be 0. */
            return 0 as libc::c_int;
        }
        if vli_cmp(curve_n.as_mut_ptr(), l_r.as_mut_ptr()) != 1 as libc::c_int || vli_cmp(curve_n.as_mut_ptr(), l_s.as_mut_ptr()) != 1 as libc::c_int
        {
            /* r, s must be < n. */
            return 0 as libc::c_int;
        }
        /* Calculate u1 and u2. */
        vli_modInv(z.as_mut_ptr(), l_s.as_mut_ptr(), curve_n.as_mut_ptr()); /* Z = s^-1 */
        ecc_bytes2native(u1.as_mut_ptr(), p_hash); /* u1 = e/s */
        vli_modMult(u1.as_mut_ptr(), u1.as_mut_ptr(), z.as_mut_ptr(), curve_n.as_mut_ptr()); /* u2 = r/s */
        vli_modMult(u2.as_mut_ptr(), l_r.as_mut_ptr(), z.as_mut_ptr(), curve_n.as_mut_ptr());
        /* Calculate l_sum = G + Q. */
        vli_set(l_sum.x.as_mut_ptr(), l_public.x.as_mut_ptr()); /* Z = x2 - x1 */
        vli_set(l_sum.y.as_mut_ptr(), l_public.y.as_mut_ptr()); /* Z = 1/Z */
        vli_set(tx.as_mut_ptr(), curve_G.x.as_mut_ptr());
        vli_set(ty.as_mut_ptr(), curve_G.y.as_mut_ptr());
        vli_modSub(z.as_mut_ptr(), l_sum.x.as_mut_ptr(), tx.as_mut_ptr(), curve_p.as_mut_ptr());
        XYcZ_add(tx.as_mut_ptr(), ty.as_mut_ptr(), l_sum.x.as_mut_ptr(), l_sum.y.as_mut_ptr());
        vli_modInv(z.as_mut_ptr(), z.as_mut_ptr(), curve_p.as_mut_ptr());
        apply_z(l_sum.x.as_mut_ptr(), l_sum.y.as_mut_ptr(), z.as_mut_ptr());
        /* Use Shamir's trick to calculate u1*G + u2*Q */
        let mut l_points: [*mut EccPoint; 4] = [0 as *mut EccPoint, &mut curve_G, &mut l_public, &mut l_sum]; /* Z = x2 - x1 */
        let mut l_numBits: uint = umax(vli_numBits(u1.as_mut_ptr()), vli_numBits(u2.as_mut_ptr())); /* Z = 1/Z */
        let mut l_point: *mut EccPoint = l_points[((vli_testBit(u1.as_mut_ptr(), l_numBits.wrapping_sub(1 as libc::c_int as libc::c_uint)) != 0)
            as libc::c_int
            | ((vli_testBit(u2.as_mut_ptr(), l_numBits.wrapping_sub(1 as libc::c_int as libc::c_uint)) != 0) as libc::c_int) << 1 as libc::c_int)
            as usize];
        vli_set(rx.as_mut_ptr(), (*l_point).x.as_mut_ptr());
        vli_set(ry.as_mut_ptr(), (*l_point).y.as_mut_ptr());
        vli_clear(z.as_mut_ptr());
        z[0 as libc::c_int as usize] = 1 as libc::c_int as uint64_t;
        let mut i: libc::c_int = 0;
        i = l_numBits.wrapping_sub(2 as libc::c_int as libc::c_uint) as libc::c_int;
        while i >= 0 as libc::c_int {
            EccPoint_double_jacobian(rx.as_mut_ptr(), ry.as_mut_ptr(), z.as_mut_ptr());
            let mut l_index: libc::c_int = (vli_testBit(u1.as_mut_ptr(), i as uint) != 0) as libc::c_int
                | ((vli_testBit(u2.as_mut_ptr(), i as uint) != 0) as libc::c_int) << 1 as libc::c_int;
            let mut l_point_0: *mut EccPoint = l_points[l_index as usize];
            if !l_point_0.is_null() {
                vli_set(tx.as_mut_ptr(), (*l_point_0).x.as_mut_ptr());
                vli_set(ty.as_mut_ptr(), (*l_point_0).y.as_mut_ptr());
                apply_z(tx.as_mut_ptr(), ty.as_mut_ptr(), z.as_mut_ptr());
                vli_modSub(tz.as_mut_ptr(), rx.as_mut_ptr(), tx.as_mut_ptr(), curve_p.as_mut_ptr());
                XYcZ_add(tx.as_mut_ptr(), ty.as_mut_ptr(), rx.as_mut_ptr(), ry.as_mut_ptr());
                vli_modMult_fast(z.as_mut_ptr(), z.as_mut_ptr(), tz.as_mut_ptr());
            }
            i -= 1
        }
        vli_modInv(z.as_mut_ptr(), z.as_mut_ptr(), curve_p.as_mut_ptr());
        apply_z(rx.as_mut_ptr(), ry.as_mut_ptr(), z.as_mut_ptr());
        /* v = x1 (mod n) */
        if vli_cmp(curve_n.as_mut_ptr(), rx.as_mut_ptr()) != 1 as libc::c_int {
            vli_sub(rx.as_mut_ptr(), rx.as_mut_ptr(), curve_n.as_mut_ptr());
        }
        /* Accept only if v == r. */
        return (vli_cmp(rx.as_mut_ptr(), l_r.as_mut_ptr()) == 0 as libc::c_int) as libc::c_int;
    }

    #[derive(Clone, PartialEq, Eq)]
    pub struct P384PublicKey([u8; 49]);

    impl P384PublicKey {
        pub fn from_bytes(b: &[u8]) -> Option<P384PublicKey> {
            if b.len() == 49 {
                Some(Self(b.try_into().unwrap()))
            } else {
                None
            }
        }

        pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
            if signature.len() == 96 {
                unsafe {
                    return ecdsa_verify(self.0.as_ptr().cast(), SHA384::hash(msg).as_ptr().cast(), signature.as_ptr().cast()) != 0;
                }
            }
            return false;
        }

        pub fn as_bytes(&self) -> &[u8; 49] {
            &self.0
        }
    }

    #[derive(Clone, PartialEq, Eq)]
    pub struct P384KeyPair(P384PublicKey, Secret<48>);

    impl P384KeyPair {
        pub fn generate() -> P384KeyPair {
            let mut kp = Self(P384PublicKey([0_u8; 49]), Secret::new());
            unsafe { ecc_make_key(kp.0 .0.as_mut_ptr().cast(), kp.1 .0.as_mut_ptr().cast()) };
            kp
        }

        pub fn from_bytes(public_bytes: &[u8], secret_bytes: &[u8]) -> Option<P384KeyPair> {
            if public_bytes.len() == 49 && secret_bytes.len() == 48 {
                Some(Self(
                    P384PublicKey(public_bytes.try_into().unwrap()),
                    Secret(secret_bytes.try_into().unwrap()),
                ))
            } else {
                None
            }
        }

        pub fn public_key(&self) -> &P384PublicKey {
            &self.0
        }

        pub fn public_key_bytes(&self) -> &[u8; 49] {
            &self.0 .0
        }

        pub fn secret_key_bytes(&self) -> Secret<48> {
            self.1.clone()
        }

        pub fn sign(&self, msg: &[u8]) -> [u8; 96] {
            let msg = SHA384::hash(msg);
            let mut sig = [0_u8; 96];
            unsafe {
                ecdsa_sign(self.1 .0.as_ptr().cast(), msg.as_ptr().cast(), sig.as_mut_ptr().cast());
            }
            sig
        }

        pub fn agree(&self, other_public: &P384PublicKey) -> Option<Secret<48>> {
            let mut k = Secret::new();
            unsafe {
                ecdh_shared_secret(other_public.0.as_ptr().cast(), self.1 .0.as_ptr().cast(), k.0.as_mut_ptr().cast());
            }
            Some(k)
        }
    }

    impl P384KeyPair {}
}

#[cfg(target_feature = "builtin_nist_ecc")]
pub use builtin::*;

#[cfg(not(target_feature = "builtin_nist_ecc"))]
pub use openssl_based::*;

#[cfg(test)]
mod tests {
    use crate::{p384::P384KeyPair, secure_eq};

    #[test]
    fn generate_sign_verify_agree() {
        let kp = P384KeyPair::generate();
        let kp2 = P384KeyPair::generate();

        let sig = kp.sign(&[0_u8; 16]);
        if !kp.public_key().verify(&[0_u8; 16], &sig) {
            panic!("ECDSA verify failed");
        }
        if kp.public_key().verify(&[1_u8; 16], &sig) {
            panic!("ECDSA verify succeeded for incorrect message");
        }

        let sec0 = kp.agree(kp2.public_key()).unwrap();
        let sec1 = kp2.agree(kp.public_key()).unwrap();
        if !secure_eq(&sec0, &sec1) {
            panic!("ECDH secrets do not match");
        }

        let pkb = kp.public_key_bytes();
        let skb = kp.secret_key_bytes();
        let kp3 = P384KeyPair::from_bytes(pkb, skb.as_ref()).unwrap();

        let pkb3 = kp3.public_key_bytes();
        let skb3 = kp3.secret_key_bytes();

        assert_eq!(pkb, pkb3);
        assert_eq!(skb.as_bytes(), skb3.as_bytes());

        let sig = kp3.sign(&[3_u8; 16]);
        if !kp.public_key().verify(&[3_u8; 16], &sig) {
            panic!("ECDSA verify failed (from key reconstructed from bytes)");
        }
    }
}
