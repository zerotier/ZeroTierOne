use std::str::FromStr;
use std::convert::TryInto;

use gcrypt::sexp::SExpression;

pub const P521_PUBLIC_KEY_SIZE: usize = 132;
pub const P521_SECRET_KEY_SIZE: usize = 66;
pub const P521_ECDSA_SIGNATURE_SIZE: usize = 132;
pub const P521_ECDH_SHARED_SECRET_SIZE: usize = 132;

/*
fn dump_sexp(exp: &SExpression) {
    if exp.len() == 1 {
        let s = exp.get_str(0);
        if s.is_ok() {
            print!("{}", s.unwrap());
        } else {
            let b = exp.get_bytes(0);
            if b.is_some() {
                print!("#{}#", crate::util::hex::to_string(b.unwrap()));
            } else {
                print!("()");
            }
        }
    } else if exp.len() > 0 {
        for i in 0..exp.len() {
            let v = exp.get(i as u32);
            if v.is_some() {
                if i == 0 {
                    print!("(");
                } else {
                    print!(" ");
                }
                dump_sexp(&v.unwrap());
            }
        }
        print!(")");
    }
}
*/

#[inline(always)]
fn hash_to_data_sexp(msg: &[u8]) -> [u8; 155] {
    let h = crate::crypto::hash::SHA512::hash(msg);
    let mut d = [0_u8; 155];
    d[0..24].copy_from_slice(b"(data(flags raw)(value #");
    let mut j = 24;
    for i in 0..64 {
        let b = h[i] as usize;
        d[j] = crate::util::hex::HEX_CHARS[b >> 4];
        d[j + 1] = crate::util::hex::HEX_CHARS[b & 0xf];
        j += 2;
    }
    d[152..155].copy_from_slice(b"#))");
    d
}

pub struct P521PublicKey {
    public_key: SExpression,
    public_key_bytes: [u8; P521_PUBLIC_KEY_SIZE],
}

/// NIST P-521 elliptic curve key pair.
/// This supports both ECDSA signing and ECDH key agreement. In practice the same key pair
/// is not used for both functions as this is considred bad practice.
pub struct P521KeyPair {
    public_key: P521PublicKey,
    secret_key_for_ecdsa: SExpression, // secret key as a private-key S-expression
    secret_key_for_ecdh: SExpression,  // the same secret key as a "data" S-expression for the weird gcrypt ECDH interface
    secret_key_bytes: [u8; P521_SECRET_KEY_SIZE],
}

impl P521KeyPair {
    /// Generate a NIST P-521 key pair.
    /// If transient is true a faster but possibly somewhat less intensive pseudo-random number
    /// generator is used. This is for ephemeral keys, and has no effect on some platforms.
    pub fn generate(transient: bool) -> Option<P521KeyPair> {
        let sexp = SExpression::from_str(if transient { "(genkey(ecc(curve nistp521)(flags nocomp transient-key)))" } else { "(genkey(ecc(curve nistp521)(flags nocomp)))" }).unwrap();
        gcrypt::pkey::generate_key(&sexp).map_or(None, |kp| {
            let pk_exp = kp.find_token("public-key");
            let sk_exp = kp.find_token("private-key");
            if pk_exp.is_some() && sk_exp.is_some() {
                let pk_exp = pk_exp.unwrap();
                let sk_exp = sk_exp.unwrap();
                let pk = pk_exp.find_token("q");
                let sk = sk_exp.find_token("d");
                if pk.is_some() && sk.is_some() {
                    let pktmp = pk.unwrap();
                    let sktmp = sk.unwrap();
                    let pk = pktmp.get_bytes(1);
                    let sk = sktmp.get_bytes(1);
                    if pk.is_some() && sk.is_some() {
                        let pk = pk.unwrap();
                        let sk = sk.unwrap();
                        let mut kp = P521KeyPair {
                            public_key: P521PublicKey {
                                public_key: pk_exp,
                                public_key_bytes: [0_u8; P521_PUBLIC_KEY_SIZE],
                            },
                            secret_key_for_ecdsa: SExpression::from_str(format!("(private-key(ecc(curve nistp521)(q #{}#)(d #{}#)))", crate::util::hex::to_string(pk), crate::util::hex::to_string(sk)).as_str()).unwrap(),
                            secret_key_for_ecdh: SExpression::from_str(format!("(data(flags raw)(value #{}#))", crate::util::hex::to_string(sk)).as_str()).unwrap(),
                            secret_key_bytes: [0_u8; P521_SECRET_KEY_SIZE],
                        };
                        kp.public_key.public_key_bytes[((P521_PUBLIC_KEY_SIZE + 1) - pk.len())..P521_PUBLIC_KEY_SIZE].copy_from_slice(&pk[1..]);
                        kp.secret_key_bytes[(P521_SECRET_KEY_SIZE - sk.len())..P521_SECRET_KEY_SIZE].copy_from_slice(sk);
                        return Some(kp);
                    }
                }
            }
            return None;
        })
    }

    /// Construct this key pair from both a public and a private key.
    pub fn from_bytes(public_bytes: &[u8], secret_bytes: &[u8]) -> Option<P521KeyPair> {
        if secret_bytes.len() != P521_SECRET_KEY_SIZE {
            return None;
        }
        let public_key = P521PublicKey::from_bytes(public_bytes);
        if public_key.is_none() {
            return None;
        }
        Some(P521KeyPair {
            public_key: public_key.unwrap(),
            secret_key_for_ecdsa: SExpression::from_str(format!("(private-key(ecc(curve nistp521)(q #04{}#)(d #{}#)))", crate::util::hex::to_string(public_bytes), crate::util::hex::to_string(secret_bytes)).as_str()).unwrap(),
            secret_key_for_ecdh: SExpression::from_str(format!("(data(flags raw)(value #{}#))", crate::util::hex::to_string(secret_bytes)).as_str()).unwrap(),
            secret_key_bytes: secret_bytes.try_into().unwrap(),
        })
    }

    #[inline(always)]
    pub fn public_key(&self) -> &P521PublicKey {
        &self.public_key
    }

    /// Get the raw ECC public "q" point for this key pair.
    /// The returned point is not compressed. To use this with other interfaces that expect a format
    /// prefix, prepend 0x04 to the beginning of this public key. This prefix is always the same in
    /// our system and so is omitted.
    #[inline(always)]
    pub fn public_key_bytes(&self) -> &[u8; P521_PUBLIC_KEY_SIZE] {
        &self.public_key.public_key_bytes
    }

    #[inline(always)]
    pub fn secret_key_bytes(&self) -> &[u8; P521_SECRET_KEY_SIZE] {
        &self.secret_key_bytes
    }

    /// Create an ECDSA signature of the input message.
    /// Message data does not need to be pre-hashed.
    pub fn sign(&self, msg: &[u8]) -> Option<[u8; P521_ECDSA_SIGNATURE_SIZE]> {
        let data = SExpression::from_str(unsafe { std::str::from_utf8_unchecked(&hash_to_data_sexp(msg)) }).unwrap();
        gcrypt::pkey::sign(&self.secret_key_for_ecdsa, &data).map_or(None, |sig| {
            let mut sig_bytes = [0_u8; P521_ECDSA_SIGNATURE_SIZE];
            if sig.find_token("r").map_or(false, |r| r.get_bytes(1).map_or(false, |r| {
                sig_bytes[(66 - r.len())..66].copy_from_slice(r);
                true
            } )) && sig.find_token("s").map_or(false, |s| s.get_bytes(1).map_or(false, |s| {
                sig_bytes[(66 + (66 - s.len()))..132].copy_from_slice(s);
                true
            })) {
                Some(sig_bytes)
            } else {
                None
            }
        })
    }

    /// Execute ECDH key agreement, returning a raw (un-hashed) shared secret.
    pub fn agree(&self, other_public: &P521PublicKey) -> Option<[u8; P521_ECDH_SHARED_SECRET_SIZE]> {
        gcrypt::pkey::encrypt(&other_public.public_key, &self.secret_key_for_ecdh).map_or(None, |k| {
            k.find_token("s").map_or(None, |s| s.get_bytes(1).map_or(None, |sb| {
                Some(sb[1..].try_into().unwrap())
            }))
        })
    }
}

impl P521PublicKey {
    /// Construct a public key from a byte serialized representation.
    /// None is returned if the input is not valid. No advanced checking such as
    /// determining if this is a point on the curve is performed.
    pub fn from_bytes(b: &[u8]) -> Option<P521PublicKey> {
        if b.len() == P521_PUBLIC_KEY_SIZE {
            Some(P521PublicKey {
                public_key: SExpression::from_str(format!("(public-key(ecc(curve nistp521)(q #04{}#)))", crate::util::hex::to_string(b)).as_str()).unwrap(),
                public_key_bytes: b.try_into().unwrap(),
            })
        } else {
            None
        }
    }

    /// Verify a signature.
    /// Message data does not need to be pre-hashed.
    pub fn verify(&self, msg: &[u8], signature: &[u8]) -> bool {
        if signature.len() == P521_ECDSA_SIGNATURE_SIZE {
            let data = SExpression::from_str(unsafe { std::str::from_utf8_unchecked(&hash_to_data_sexp(msg)) }).unwrap();
            let sig = SExpression::from_str(format!("(sig-val(ecdsa(r #{}#)(s #{}#)))", crate::util::hex::to_string(&signature[0..66]), crate::util::hex::to_string(&signature[66..132])).as_str()).unwrap();
            gcrypt::pkey::verify(&self.public_key, &data, &sig).is_ok()
        } else {
            false
        }
    }

    #[inline(always)]
    pub fn public_key_bytes(&self) -> &[u8; P521_PUBLIC_KEY_SIZE] {
        &self.public_key_bytes
    }
}

impl PartialEq for P521PublicKey {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.public_key_bytes.eq(&other.public_key_bytes)
    }
}

impl Eq for P521PublicKey {}

impl Clone for P521PublicKey {
    fn clone(&self) -> Self {
        P521PublicKey::from_bytes(&self.public_key_bytes).unwrap()
    }
}

#[cfg(test)]
mod tests {
    use crate::crypto::p521::P521KeyPair;

    #[test]
    fn generate_sign_verify_agree() {
        let kp = P521KeyPair::generate(false).unwrap();
        let kp2 = P521KeyPair::generate(false).unwrap();

        let sig = kp.sign(&[0_u8]).unwrap();
        if !kp.public_key().verify(&[0_u8], &sig) {
            panic!("ECDSA verify failed");
        }
        if kp.public_key().verify(&[1_u8], &sig) {
            panic!("ECDSA verify succeeded for incorrect message");
        }

        let sec0 = kp.agree(kp2.public_key()).unwrap();
        let sec1 = kp2.agree(kp.public_key()).unwrap();
        if !sec0.eq(&sec1) {
            panic!("ECDH secrets do not match");
        }

        let kp3 = P521KeyPair::from_bytes(kp.public_key_bytes(), kp.secret_key_bytes()).unwrap();
        let sig = kp3.sign(&[3_u8]).unwrap();
        if !kp.public_key().verify(&[3_u8], &sig) {
            panic!("ECDSA verify failed (from key reconstructed from bytes)");
        }
    }
}
