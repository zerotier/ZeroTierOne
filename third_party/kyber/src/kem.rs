#[cfg(not(feature = "KATs"))]
use crate::rng::randombytes;
use crate::{error::KyberError, indcpa::*, params::*, symmetric::*, verify::*};
use rand_core::{CryptoRng, RngCore};

// Name:        crypto_kem_keypair
//
// Description: Generates public and private key
//              for CCA-secure Kyber key encapsulation mechanism
//
// Arguments:   - [u8] pk: output public key (an already allocated array of CRYPTO_PUBLICKEYBYTES bytes)
//              - [u8] sk: output private key (an already allocated array of CRYPTO_SECRETKEYBYTES bytes)
pub fn crypto_kem_keypair<R>(pk: &mut [u8], sk: &mut [u8], _rng: &mut R, _seed: Option<(&[u8], &[u8])>)
where
    R: RngCore + CryptoRng,
{
    const PK_START: usize = KYBER_SECRETKEYBYTES - (2 * KYBER_SYMBYTES);
    const SK_START: usize = KYBER_SECRETKEYBYTES - KYBER_SYMBYTES;
    const END: usize = KYBER_INDCPA_PUBLICKEYBYTES + KYBER_INDCPA_SECRETKEYBYTES;

    indcpa_keypair(pk, sk, _seed, _rng);

    sk[KYBER_INDCPA_SECRETKEYBYTES..END].copy_from_slice(&pk[..KYBER_INDCPA_PUBLICKEYBYTES]);
    hash_h(&mut sk[PK_START..], pk, KYBER_PUBLICKEYBYTES);

    #[cfg(feature = "KATs")]
    sk[SK_START..].copy_from_slice(&_seed.expect("KATs feature only for testing").1);

    #[cfg(not(feature = "KATs"))]
    randombytes(&mut sk[SK_START..], KYBER_SYMBYTES, _rng);
}

// Name:        crypto_kem_enc
//
// Description: Generates cipher text and shared
//              secret for given public key
//
// Arguments:   - [u8] ct:       output cipher text (an already allocated array of CRYPTO_CIPHERTEXTBYTES bytes)
//              - [u8] ss:       output shared secret (an already allocated array of CRYPTO_BYTES bytes)
//              - const [u8] pk: input public key (an already allocated array of CRYPTO_PUBLICKEYBYTES bytes)
pub fn crypto_kem_enc<R>(ct: &mut [u8], ss: &mut [u8], pk: &[u8], _rng: &mut R, _seed: Option<&[u8]>)
where
    R: RngCore + CryptoRng,
{
    let mut kr = [0u8; 2 * KYBER_SYMBYTES];
    let mut buf = [0u8; 2 * KYBER_SYMBYTES];
    let mut randbuf = [0u8; 2 * KYBER_SYMBYTES];

    #[cfg(not(feature = "KATs"))]
    randombytes(&mut randbuf, KYBER_SYMBYTES, _rng);

    // Deterministic randbuf for KAT's
    #[cfg(feature = "KATs")]
    randbuf[..KYBER_SYMBYTES].copy_from_slice(&_seed.expect("KATs feature only works with `cargo test`"));

    // Don't release system RNG output
    hash_h(&mut buf, &randbuf, KYBER_SYMBYTES);

    // Multitarget countermeasure for coins + contributory KEM
    hash_h(&mut buf[KYBER_SYMBYTES..], pk, KYBER_PUBLICKEYBYTES);
    hash_g(&mut kr, &buf, 2 * KYBER_SYMBYTES);

    // coins are in kr[KYBER_SYMBYTES..]
    indcpa_enc(ct, &buf, pk, &kr[KYBER_SYMBYTES..]);

    // overwrite coins in kr with H(c)
    hash_h(&mut kr[KYBER_SYMBYTES..], ct, KYBER_CIPHERTEXTBYTES);

    // hash concatenation of pre-k and H(c) to k
    kdf(ss, &kr, 2 * KYBER_SYMBYTES);
}

// Name:        crypto_kem_dec
//
// Description: Generates shared secret for given
//              cipher text and private key
//
// Arguments:   - [u8] ss:       output shared secret (an already allocated array of CRYPTO_BYTES bytes)
//              - const [u8] ct: input cipher text (an already allocated array of CRYPTO_CIPHERTEXTBYTES bytes)
//              - const [u8] sk: input private key (an already allocated array of CRYPTO_SECRETKEYBYTES bytes)
//
// On failure, ss will contain a pseudo-random value.
pub fn crypto_kem_dec(ss: &mut [u8], ct: &[u8], sk: &[u8]) -> Result<(), KyberError> {
    let mut buf = [0u8; 2 * KYBER_SYMBYTES];
    let mut kr = [0u8; 2 * KYBER_SYMBYTES];
    let mut cmp = [0u8; KYBER_CIPHERTEXTBYTES];
    let mut pk = [0u8; KYBER_INDCPA_PUBLICKEYBYTES + 2 * KYBER_SYMBYTES];

    pk.copy_from_slice(&sk[KYBER_INDCPA_SECRETKEYBYTES..]);

    indcpa_dec(&mut buf, ct, sk);

    // Multitarget countermeasure for coins + contributory KEM
    const START: usize = KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES;
    const END: usize = KYBER_SECRETKEYBYTES - KYBER_SYMBYTES;
    buf[KYBER_SYMBYTES..].copy_from_slice(&sk[START..END]);
    hash_g(&mut kr, &buf, 2 * KYBER_SYMBYTES);

    // coins are in kr[KYBER_SYMBYTES..]
    indcpa_enc(&mut cmp, &buf, &pk, &kr[KYBER_SYMBYTES..]);
    let fail = verify(ct, &cmp, KYBER_CIPHERTEXTBYTES);
    // overwrite coins in kr with H(c)
    hash_h(&mut kr[KYBER_SYMBYTES..], ct, KYBER_CIPHERTEXTBYTES);
    // Overwrite pre-k with z on re-encryption failure
    cmov(&mut kr, &sk[END..], KYBER_SYMBYTES, fail);
    // hash concatenation of pre-k and H(c) to k
    kdf(ss, &kr, 2 * KYBER_SYMBYTES);

    match fail {
        0 => Ok(()),
        _ => Err(KyberError::Decapsulation),
    }
}
