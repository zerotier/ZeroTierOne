// This file is part of sidh-rs.
// Copyright (c) 2017 Erkan Tairi
// See LICENSE for licensing information.
//
// Author:
// - Erkan Tairi <erkan.tairi@gmail.com>
//

use crate::sidhp751::field::ExtensionFieldElement;
use crate::sidhp751::curve::{ProjectiveCurveParameters, ProjectivePoint};
use crate::sidhp751::isogeny::*;
use crate::sidhp751::constants::*;
use crate::sidhp751::fp::*;
#[allow(unused_imports)]
use crate::random::SecureRandom;

use heapless::Vec;

use std::fmt::Debug;
use std::ops::Neg;

use rand_core::{RngCore, CryptoRng};

#[cfg(test)]
use quickcheck::{Arbitrary, Gen, QuickCheck};

/// The secret key size, in bytes.
pub const SECRET_KEY_SIZE: usize = 48;
/// The public key size, in bytes.
pub const PUBLIC_KEY_SIZE: usize = 564;
/// The shared secret size, in bytes.
pub const SHARED_SECRET_SIZE: usize = 188;

const MAX_INT_POINTS_ALICE: usize = 8;
const MAX_INT_POINTS_BOB: usize = 10;

const MAX_ALICE: usize = 185;
/// Alice's isogeny strategy.
pub const ALICE_ISOGENY_STRATEGY: [u8; MAX_ALICE] = [0, 1, 1, 2, 2, 2, 3, 4, 4, 4, 4, 5, 5,
	        6, 7, 8, 8, 9, 9, 9, 9, 9, 9, 9, 12, 11, 12, 12, 13, 14, 15, 16, 16, 16, 16,
	        16, 16, 17, 17, 18, 18, 17, 21, 17, 18, 21, 20, 21, 21, 21, 21, 21, 22, 25, 25,
	        25, 26, 27, 28, 28, 29, 30, 31, 32, 32, 32, 32, 32, 32, 32, 33, 33, 33, 35, 36,
	        36, 33, 36, 35, 36, 36, 35, 36, 36, 37, 38, 38, 39, 40, 41, 42, 38, 39, 40, 41,
	        42, 40, 46, 42, 43, 46, 46, 46, 46, 48, 48, 48, 48, 49, 49, 48, 53, 54, 51, 52,
	        53, 54, 55, 56, 57, 58, 59, 59, 60, 62, 62, 63, 64, 64, 64, 64, 64, 64, 64, 64,
	        65, 65, 65, 65, 65, 66, 67, 65, 66, 67, 66, 69, 70, 66, 67, 66, 69, 70, 69, 70,
	        70, 71, 72, 71, 72, 72, 74, 74, 75, 72, 72, 74, 74, 75, 72, 72, 74, 75, 75, 72,
	        72, 74, 75, 75, 77, 77, 79, 80, 80, 82];


const MAX_BOB: usize = 239;
/// Bob's isogeny strategy.
pub const BOB_ISOGENY_STRATEGY: [u8; MAX_BOB] = [0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6,
	        7, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 12, 12, 12, 12, 12, 12, 13, 14, 14, 15, 16,
	        16, 16, 16, 16, 17, 16, 16, 17, 19, 19, 20, 21, 22, 22, 22, 22, 22, 22, 22, 22,
	        22, 22, 24, 24, 25, 27, 27, 28, 28, 29, 28, 29, 28, 28, 28, 30, 28, 28, 28, 29,
	        30, 33, 33, 33, 33, 34, 35, 37, 37, 37, 37, 38, 38, 37, 38, 38, 38, 38, 38, 39,
	        43, 38, 38, 38, 38, 43, 40, 41, 42, 43, 48, 45, 46, 47, 47, 48, 49, 49, 49, 50,
	        51, 50, 49, 49, 49, 49, 51, 49, 53, 50, 51, 50, 51, 51, 51, 52, 55, 55, 55, 56,
	        56, 56, 56, 56, 58, 58, 61, 61, 61, 63, 63, 63, 64, 65, 65, 65, 65, 66, 66, 65,
	        65, 66, 66, 66, 66, 66, 66, 66, 71, 66, 73, 66, 66, 71, 66, 73, 66, 66, 71, 66,
	        73, 68, 68, 71, 71, 73, 73, 73, 75, 75, 78, 78, 78, 80, 80, 80, 81, 81, 82, 83,
	        84, 85, 86, 86, 86, 86, 86, 87, 86, 88, 86, 86, 86, 86, 88, 86, 88, 86, 86, 86,
	        88, 88, 86, 86, 86, 93, 90, 90, 92, 92, 92, 93, 93, 93, 93, 93, 97, 97, 97, 97,
	        97, 97];

/// Alice's public key.
#[allow(non_snake_case)]
#[derive(Copy, Clone)]
pub struct SIDHPublicKeyAlice {
    pub affine_xP  : ExtensionFieldElement,
    pub affine_xQ  : ExtensionFieldElement,
    pub affine_xQmP: ExtensionFieldElement,
}

impl SIDHPublicKeyAlice {
    /// Read a public key from a byte slice. The input must be at least 564 bytes long.
    #[allow(non_snake_case)]
    pub fn from_bytes(bytes: &[u8]) -> SIDHPublicKeyAlice {
        assert!(bytes.len() >= 564, "Too short input to SIDH public key from_bytes, expected 564 bytes");
        let affine_xP = ExtensionFieldElement::from_bytes(&bytes[0..188]);
        let affine_xQ = ExtensionFieldElement::from_bytes(&bytes[188..376]);
        let affine_xQmP = ExtensionFieldElement::from_bytes(&bytes[376..564]);
        SIDHPublicKeyAlice{ affine_xP, affine_xQ, affine_xQmP }
    }
    /// Write a public key to a byte slice. The output will be 564 bytes long.
    pub fn to_bytes(&self) -> [u8; 564] {
        let mut bytes = [0u8; 564];
        bytes[0..188].clone_from_slice(&self.affine_xP.to_bytes());
        bytes[188..376].clone_from_slice(&self.affine_xQ.to_bytes());
        bytes[376..564].clone_from_slice(&self.affine_xQmP.to_bytes());
        bytes
    }
}

/// Bob's public key.
#[allow(non_snake_case)]
#[derive(Copy, Clone)]
pub struct SIDHPublicKeyBob {
    pub affine_xP  : ExtensionFieldElement,
    pub affine_xQ  : ExtensionFieldElement,
    pub affine_xQmP: ExtensionFieldElement,
}

impl SIDHPublicKeyBob {
    /// Read a public key from a byte slice. The input must be at least 564 bytes long.
    #[allow(non_snake_case)]
    pub fn from_bytes(bytes: &[u8]) -> SIDHPublicKeyBob {
        assert!(bytes.len() >= 564, "Too short input to SIDH public key from_bytes, expected 564 bytes");
        let affine_xP = ExtensionFieldElement::from_bytes(&bytes[0..188]);
        let affine_xQ = ExtensionFieldElement::from_bytes(&bytes[188..376]);
        let affine_xQmP = ExtensionFieldElement::from_bytes(&bytes[376..564]);
        SIDHPublicKeyBob{ affine_xP, affine_xQ, affine_xQmP }
    }
    /// Write a public key to a byte slice. The output will be 564 bytes long.
    pub fn to_bytes(&self) -> [u8; 564] {
        let mut bytes = [0u8; 564];
        bytes[0..188].clone_from_slice(&self.affine_xP.to_bytes());
        bytes[188..376].clone_from_slice(&self.affine_xQ.to_bytes());
        bytes[376..564].clone_from_slice(&self.affine_xQmP.to_bytes());
        bytes
    }
}

/// Alice's secret key.
#[derive(Copy, Clone)]
pub struct SIDHSecretKeyAlice {
    pub scalar: [u8; SECRET_KEY_SIZE],
}

impl Debug for SIDHSecretKeyAlice {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "SIDHSecretKeyAlice(scalar: {:?})", &self.scalar[..])
    }
}

#[cfg(test)]
impl Arbitrary for SIDHSecretKeyAlice {
    fn arbitrary(_g: &mut Gen) -> SIDHSecretKeyAlice {
        let mut rng = SecureRandom::get();
        let (_, alice_secret_key) = generate_alice_keypair(&mut rng);
        alice_secret_key
    }
}

impl SIDHSecretKeyAlice {
    /// Compute the corresponding public key for the given secret key.
    #[allow(non_snake_case)]
    pub fn public_key(&self) -> SIDHPublicKeyAlice {
        let mut xP = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PB);  // = ( x_P : 1) = x(P_B)
        let mut xQ = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PB);  //
        xQ.X = (&xQ.X).neg();                                                      // = (-x_P : 1) = x(Q_B)
        let mut xQmP = ProjectivePoint::distort_and_difference(&AFFINE_X_PB); // = x(Q_B - P_B)

        let mut xR = ProjectivePoint::secret_point(&AFFINE_X_PA, &AFFINE_Y_PA, &self.scalar[..]);

        // Starting curve has a = 0, so (A:C) = (0,1).
        let current_curve = ProjectiveCurveParameters{ A: ExtensionFieldElement::zero(), C: ExtensionFieldElement::one() }; 
        let (mut current_curve, firstPhi) = FirstFourIsogeny::compute_first_four_isogeny(&current_curve);

        xP = firstPhi.eval(&xP);
        xQ = firstPhi.eval(&xQ);
        xQmP = firstPhi.eval(&xQmP);
        xR = firstPhi.eval(&xR);
        
        let mut points: Vec<ProjectivePoint, MAX_INT_POINTS_ALICE> = Vec::new();
        let mut indices: Vec<usize, MAX_INT_POINTS_ALICE> = Vec::new();
        let mut i: usize = 0;
        let mut phi: FourIsogeny;
        for j in 1..MAX_ALICE {
            while i < MAX_ALICE-j {
                points.push(xR).unwrap();
                indices.push(i).unwrap();
                let k = ALICE_ISOGENY_STRATEGY[MAX_ALICE-i-j];
                xR = xR.pow2k(&current_curve, (2*k) as u32);
                i = i + k as usize;
            }
            assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xR)};

            for k in 0..points.len() {
                points[k] = phi.eval(&points[k]);
            }

            xP = phi.eval(&xP);
            xQ = phi.eval(&xQ);
            xQmP = phi.eval(&xQmP);

            // Pop xR from pointsm and i from indices.
            xR = points.pop().unwrap();
            i = indices.pop().unwrap();
        }

        assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xR)};

        xP = phi.eval(&xP);
        xQ = phi.eval(&xQ);
        xQmP = phi.eval(&xQmP);

        let (invZP, invZQ, invZQmP) = ExtensionFieldElement::batch3_inv(&xP.Z, &xQ.Z, &xQmP.Z);
        let affine_xP = &xP.X * &invZP;
        let affine_xQ = &xQ.X * &invZQ;
        let affine_xQmP = &xQmP.X * &invZQmP;

        SIDHPublicKeyAlice{ affine_xP, affine_xQ, affine_xQmP }
    }

    /// Compute (Alice's view of) a shared secret using Alice's secret key and Bob's public key.
    #[allow(non_snake_case)]
    pub fn shared_secret(&self, bob_public: &SIDHPublicKeyBob) -> [u8; SHARED_SECRET_SIZE] {
        let current_curve = ProjectiveCurveParameters::recover_curve_parameters(&bob_public.affine_xP, &bob_public.affine_xQ, &bob_public.affine_xQmP);
        let xP = ProjectivePoint::from_affine(&bob_public.affine_xP);
        let xQ = ProjectivePoint::from_affine(&bob_public.affine_xQ);
        let xQmP = ProjectivePoint::from_affine(&bob_public.affine_xQmP);
        let mut xR = ProjectivePoint::right_to_left_ladder(&xP, &xQ, &xQmP, &current_curve, &self.scalar[..]);

        let (mut current_curve, firstPhi) = FirstFourIsogeny::compute_first_four_isogeny(&current_curve);
        xR = firstPhi.eval(&xR);

        let mut points: Vec<ProjectivePoint, MAX_INT_POINTS_ALICE> = Vec::new();
        let mut indices: Vec<usize, MAX_INT_POINTS_ALICE> = Vec::new();
        let mut i: usize = 0;
        let mut phi: FourIsogeny;
        for j in 1..MAX_ALICE {
            while i < MAX_ALICE-j {
                points.push(xR).unwrap();
                indices.push(i).unwrap();
                let k = ALICE_ISOGENY_STRATEGY[MAX_ALICE-i-j];
                xR = xR.pow2k(&current_curve, (2*k) as u32);
                i = i + k as usize;
            }
            assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xR)};

            for k in 0..points.len() {
                points[k] = phi.eval(&points[k]);
            }

            // Pop xR from pointsm and i from indices.
		    xR = points.pop().unwrap();
            i = indices.pop().unwrap();
        }

        assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xR)};

        let j_inv = current_curve.j_invariant();
        let shared_secret = j_inv.to_bytes();
        shared_secret
    }
}

/// Bob's secret key.
#[derive(Copy, Clone)]
pub struct SIDHSecretKeyBob {
    pub scalar: [u8; SECRET_KEY_SIZE],
}

impl Debug for SIDHSecretKeyBob {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "SIDHSecretKeyBob(scalar: {:?})", &self.scalar[..])
    }
}

#[cfg(test)]
impl Arbitrary for SIDHSecretKeyBob {
    fn arbitrary(_g: &mut Gen) -> SIDHSecretKeyBob {
        let mut rng = SecureRandom::get();
        let (_, bob_secret_key) = generate_bob_keypair(&mut rng);
        bob_secret_key
    }
}

impl SIDHSecretKeyBob {
    /// Compute the public key corresponding to the secret key.
    #[allow(non_snake_case)]
    pub fn public_key(&self) -> SIDHPublicKeyBob {
        let mut xP = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PA);  // = ( x_P : 1) = x(P_A)
        let mut xQ = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PA);  //
        xQ.X = (&xQ.X).neg();                                                      // = (-x_P : 1) = x(Q_A)
        let mut xQmP = ProjectivePoint::distort_and_difference(&AFFINE_X_PA); // = x(Q_B - P_B)

        let mut xR = ProjectivePoint::secret_point(&AFFINE_X_PB, &AFFINE_Y_PB, &self.scalar[..]);

        // Starting curve has a = 0, so (A:C) = (0,1).
        let mut current_curve = ProjectiveCurveParameters{ A: ExtensionFieldElement::zero(), C: ExtensionFieldElement::one() };

        let mut points: Vec<ProjectivePoint, MAX_INT_POINTS_BOB> = Vec::new();
        let mut indices: Vec<usize, MAX_INT_POINTS_BOB> = Vec::new();
        let mut i: usize = 0;
        let mut phi: ThreeIsogeny;
        for j in 1..MAX_BOB {
            while i < MAX_BOB-j {
                points.push(xR).unwrap();
                indices.push(i).unwrap();
                let k = BOB_ISOGENY_STRATEGY[MAX_BOB-i-j];
                xR = xR.pow3k(&current_curve, k as u32);
                i = i + k as usize;
            }
             assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xR)};

            for k in 0..points.len() {
                points[k] = phi.eval(&points[k]);
            }

            xP = phi.eval(&xP);
            xQ = phi.eval(&xQ);
            xQmP = phi.eval(&xQmP);

            // Pop xR from points and i from indices.
		    xR = points.pop().unwrap();
            i = indices.pop().unwrap();
        }

        assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xR)};

        xP = phi.eval(&xP);
        xQ = phi.eval(&xQ);
        xQmP = phi.eval(&xQmP);

        let (invZP, invZQ, invZQmP) = ExtensionFieldElement::batch3_inv(&xP.Z, &xQ.Z, &xQmP.Z);
        let affine_xP = &xP.X * &invZP;
        let affine_xQ = &xQ.X * &invZQ;
        let affine_xQmP = &xQmP.X * &invZQmP;

        SIDHPublicKeyBob{ affine_xP, affine_xQ, affine_xQmP }
    }

    /// Compute (Bob's view of) a shared secret using Bob's secret key and Alice's public key.
    #[allow(non_snake_case)]
    pub fn shared_secret(&self, alice_public: &SIDHPublicKeyAlice) -> [u8; SHARED_SECRET_SIZE] {
        let mut current_curve = ProjectiveCurveParameters::recover_curve_parameters(&alice_public.affine_xP, &alice_public.affine_xQ, &alice_public.affine_xQmP);
        let xP = ProjectivePoint::from_affine(&alice_public.affine_xP);
        let xQ = ProjectivePoint::from_affine(&alice_public.affine_xQ);
        let xQmP = ProjectivePoint::from_affine(&alice_public.affine_xQmP);
        let mut xR = ProjectivePoint::right_to_left_ladder(&xP, &xQ, &xQmP, &current_curve, &self.scalar[..]);

        let mut points: Vec<ProjectivePoint, MAX_INT_POINTS_BOB> = Vec::new();
        let mut indices: Vec<usize, MAX_INT_POINTS_BOB> = Vec::new();
        let mut i: usize = 0;
        let mut phi: ThreeIsogeny;
        for j in 1..MAX_BOB {
            while i < MAX_BOB-j {
                points.push(xR).unwrap();
                indices.push(i).unwrap();
                let k = BOB_ISOGENY_STRATEGY[MAX_BOB-i-j];
                xR = xR.pow3k(&current_curve, k as u32);
                i = i + k as usize;
            }
            assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xR)};

            for k in 0..points.len() {
                points[k] = phi.eval(&points[k]);
            }

            // Pop xR from points and i from indices.
		    xR = points.pop().unwrap();
            i = indices.pop().unwrap();
        }

        assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xR)};

        let j_inv = current_curve.j_invariant();
        let shared_secret = j_inv.to_bytes();
        shared_secret
    }
}

/// Generate a keypair for "Alice". Note that because this library does not
/// implement SIDH validation, each keypair should be used for at most one
/// shared secret computation.
pub fn generate_alice_keypair<R: RngCore + CryptoRng>(rng: &mut R) -> (SIDHPublicKeyAlice, SIDHSecretKeyAlice) {
    let mut scalar = [0u8; SECRET_KEY_SIZE];
    rng.fill_bytes(&mut scalar[..]);

    // Bit-twiddle to ensure scalar is in 2*[0,2^371):
    scalar[47] = 0;
    scalar[46] &= 15; // Clear high bits, so scalar < 2^372.
    scalar[0] &= 254; // Clear low bit, so scalar is even.

    // We actually want scalar in 2*(0,2^371), but the above procedure
	// generates 0 with probability 2^(-371), which isn't worth checking
	// for.
    let secret_key = SIDHSecretKeyAlice{ scalar };
    let public_key = secret_key.public_key();

    (public_key, secret_key)
}

/// Generate a keypair for "Bob". Note that because this library does not
/// implement SIDH validation, each keypair should be used for at most one
/// shared secret computation.
pub fn generate_bob_keypair<R: RngCore + CryptoRng>(rng: &mut R) -> (SIDHPublicKeyBob, SIDHSecretKeyBob) {
    let mut scalar = [0u8; SECRET_KEY_SIZE];
    // Perform rejection sampling to obtain a random value in [0,3^238]:
    let mut ok: u32 = 1;
    for _ in 0..102 {
        rng.fill_bytes(&mut scalar[..]);
        // Mask the high bits to obtain a uniform value in [0,2^378):
        scalar[47] &= 3;
        // Accept if scalar < 3^238 (this happens with probability ~0.5828).
        checklt238(&scalar, &mut ok);
        if ok == 0 { break; }
    }
    // ok is nonzero if all 102 trials failed.
	// This happens with probability 0.41719...^102 < 2^(-128), i.e., never.
    if ok != 0 { panic!("All 102 trials failed!"); }
    
    // Multiply by 3 to get a scalar in 3*[0,3^238):
    mulby3(&mut scalar);

    // We actually want scalar in 2*(0,2^371), but the above procedure
	// generates 0 with probability 3^(-238), which isn't worth checking
	// for.
    let secret_key = SIDHSecretKeyBob{ scalar };
    let public_key = secret_key.public_key();

    (public_key, secret_key)
}


#[cfg(test)]
mod test {
    use super::*;
    #[allow(unused_imports)]
    use std::time::SystemTime;

    // Perform Alice's (2-isogeny) key generation, using the slow but simple multiplication-based strategy.
    //
    // This function just exists to ensure that the fast isogeny-tree strategy works correctly.
    #[allow(non_snake_case)]
    pub fn alice_keygen_slow(secret_key: &SIDHSecretKeyAlice) -> SIDHPublicKeyAlice {
        let mut xP = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PB);  // = ( x_P : 1) = x(P_B)
        let mut xQ = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PB);  //
        xQ.X = (&xQ.X).neg();                                                      // = (-x_P : 1) = x(Q_B)
        let mut xQmP = ProjectivePoint::distort_and_difference(&AFFINE_X_PB); // = x(Q_B - P_B)

        let mut xR = ProjectivePoint::secret_point(&AFFINE_X_PA, &AFFINE_Y_PA, &secret_key.scalar[..]);
        // Starting curve has a = 0, so (A:C) = (0,1).
        let current_curve = ProjectiveCurveParameters{ A: ExtensionFieldElement::zero(), C: ExtensionFieldElement::one() };

        let (mut current_curve, firstPhi) = FirstFourIsogeny::compute_first_four_isogeny(&current_curve);

        xP = firstPhi.eval(&xP);
        xQ = firstPhi.eval(&xQ);
        xQmP = firstPhi.eval(&xQmP);
        xR = firstPhi.eval(&xR);

        let mut phi: FourIsogeny;
        // rev() makes the loop go from 368 down to 0.
        for e in (0..(372 - 4 + 1)).rev().step_by(2) {
            let xS = xR.pow2k(&current_curve, e as u32);
            assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xS)};

            xR = phi.eval(&xR);
            xP = phi.eval(&xP);
            xQ = phi.eval(&xQ);
            xQmP = phi.eval(&xQmP);
        }

        let (invZP, invZQ, invZQmP) = ExtensionFieldElement::batch3_inv(&xP.Z, &xQ.Z, &xQmP.Z);
        let affine_xP = &xP.X * &invZP;
        let affine_xQ = &xQ.X * &invZQ;
        let affine_xQmP = &xQmP.X * &invZQmP;

        SIDHPublicKeyAlice{ affine_xP, affine_xQ, affine_xQmP }
    }

    // Perform Bob's (3-isogeny) key generation, using the slow but simple multiplication-based strategy.
    //
    // This function just exists to ensure that the fast isogeny-tree strategy works correctly.
    #[allow(non_snake_case)]
    pub fn bob_keygen_slow(secret_key: &SIDHSecretKeyBob) -> SIDHPublicKeyBob {
        let mut xP = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PA);  // = ( x_P : 1) = x(P_A)
        let mut xQ = ProjectivePoint::from_affine_prime_field(&AFFINE_X_PA);  //
        xQ.X = (&xQ.X).neg();                                                      // = (-x_P : 1) = x(Q_A)
        let mut xQmP = ProjectivePoint::distort_and_difference(&AFFINE_X_PA); // = x(Q_B - P_B)

        let mut xR = ProjectivePoint::secret_point(&AFFINE_X_PB, &AFFINE_Y_PB, &secret_key.scalar[..]);
        // Starting curve has a = 0, so (A:C) = (0,1).
        let mut current_curve = ProjectiveCurveParameters{ A: ExtensionFieldElement::zero(), C: ExtensionFieldElement::one() };

        let mut phi: ThreeIsogeny;
        // rev() makes the loop go from 238 down to 0.
        for e in (0..MAX_BOB).rev() {
            let xS = xR.pow3k(&current_curve, e as u32);
            assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xS)};

            xR = phi.eval(&xR);
            xP = phi.eval(&xP);
            xQ = phi.eval(&xQ);
            xQmP = phi.eval(&xQmP);
        }

        let (invZP, invZQ, invZQmP) = ExtensionFieldElement::batch3_inv(&xP.Z, &xQ.Z, &xQmP.Z);
        let affine_xP = &xP.X * &invZP;
        let affine_xQ = &xQ.X * &invZQ;
        let affine_xQmP = &xQmP.X * &invZQmP;

        SIDHPublicKeyBob{ affine_xP, affine_xQ, affine_xQmP }
    }

    // Perform Alice's key agreement, using the slow but simple multiplication-based strategy.
    //
    // This function just exists to ensure that the fast isogeny-tree strategy works correctly.
    #[allow(non_snake_case)]
    pub fn alice_shared_secret_slow(bob_public: &SIDHPublicKeyBob, alice_secret: &SIDHSecretKeyAlice) -> [u8; SHARED_SECRET_SIZE] {
        let current_curve = ProjectiveCurveParameters::recover_curve_parameters(&bob_public.affine_xP, &bob_public.affine_xQ, &bob_public.affine_xQmP);
        let xP = ProjectivePoint::from_affine(&bob_public.affine_xP);
        let xQ = ProjectivePoint::from_affine(&bob_public.affine_xQ);
        let xQmP = ProjectivePoint::from_affine(&bob_public.affine_xQmP);
        
        let mut xR = ProjectivePoint::three_point_ladder(&xP, &xQ, &xQmP, &current_curve, &alice_secret.scalar[..]);
        
        let (mut current_curve, firstPhi) = FirstFourIsogeny::compute_first_four_isogeny(&current_curve);
        xR = firstPhi.eval(&xR);

        let mut phi: FourIsogeny;
        // rev() makes the loop go from 368 down to 2.
        for e in (2..(372 - 4 + 1)).rev().step_by(2) {
            let xS = xR.pow2k(&current_curve, e as u32);
            assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xS)};

            xR = phi.eval(&xR);
        }

        assign!{(current_curve, phi) = FourIsogeny::compute_four_isogeny(&xR)};

        let j_inv = current_curve.j_invariant();
        let shared_secret = j_inv.to_bytes();
        shared_secret
    }

    // Perform Bob's key agreement, using the slow but simple multiplication-based strategy.
    //
    // This function just exists to ensure that the fast isogeny-tree strategy works correctly.
    #[allow(non_snake_case)]
    pub fn bob_shared_secret_slow(alice_public: &SIDHPublicKeyAlice, bob_secret: &SIDHSecretKeyBob) -> [u8; SHARED_SECRET_SIZE] {
        let mut current_curve = ProjectiveCurveParameters::recover_curve_parameters(&alice_public.affine_xP, &alice_public.affine_xQ, &alice_public.affine_xQmP);
        let xP = ProjectivePoint::from_affine(&alice_public.affine_xP);
        let xQ = ProjectivePoint::from_affine(&alice_public.affine_xQ);
        let xQmP = ProjectivePoint::from_affine(&alice_public.affine_xQmP);
        
        let mut xR = ProjectivePoint::three_point_ladder(&xP, &xQ, &xQmP, &current_curve, &bob_secret.scalar[..]);

        let mut phi: ThreeIsogeny;
        // rev() makes the loop go from 239 down to 1.
        for e in (1..MAX_BOB).rev() {
            let xS = xR.pow3k(&current_curve, e as u32);
            assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xS)};

            xR = phi.eval(&xR);
        }

        assign!{(current_curve, phi) = ThreeIsogeny::compute_three_isogeny(&xR)};

        let j_inv = current_curve.j_invariant();
        let shared_secret = j_inv.to_bytes();
        shared_secret
    }

    #[test]
    fn multiply_by_three() {
        // sage: repr((3^238 -1).digits(256))
        let mut three238_minus1: [u8; 48] = [248, 132, 131, 130, 138, 113, 205, 237, 20, 122, 66, 212, 191, 53, 59, 115, 56, 207, 215, 148, 207, 41, 130, 248, 214, 42, 124, 12, 153, 108, 197, 99, 199, 34, 66, 143, 126, 168, 88, 184, 245, 234, 37, 181, 198, 201, 84, 2];
        // sage: repr((3*(3^238 -1)).digits(256))
        let three_times_three238_minus1: [u8; 48] = [232, 142, 138, 135, 159, 84, 104, 201, 62, 110, 199, 124, 63, 161, 177, 89, 169, 109, 135, 190, 110, 125, 134, 233, 132, 128, 116, 37, 203, 69, 80, 43, 86, 104, 198, 173, 123, 249, 9, 41, 225, 192, 113, 31, 84, 93, 254, 6];

        mulby3(&mut three238_minus1);

        assert!(three238_minus1.iter().zip(three_times_three238_minus1.iter()).all(|(a, b)| a == b), 
                "\nExpected\n{:?}\nfound\n{:?}", &three_times_three238_minus1[..], &three238_minus1[..]);
    }

    #[test]
    fn check_less_than_three238() {
        let three238_minus1: [u8; 48] = [248, 132, 131, 130, 138, 113, 205, 237, 20, 122, 66, 212, 191, 53, 59, 115, 56, 207, 215, 148, 207, 41, 130, 248, 214, 42, 124, 12, 153, 108, 197, 99, 199, 34, 66, 143, 126, 168, 88, 184, 245, 234, 37, 181, 198, 201, 84, 2];
        let three238: [u8; 48] = [249, 132, 131, 130, 138, 113, 205, 237, 20, 122, 66, 212, 191, 53, 59, 115, 56, 207, 215, 148, 207, 41, 130, 248, 214, 42, 124, 12, 153, 108, 197, 99, 199, 34, 66, 143, 126, 168, 88, 184, 245, 234, 37, 181, 198, 201, 84, 2];
        let three238_plus1: [u8; 48] = [250, 132, 131, 130, 138, 113, 205, 237, 20, 122, 66, 212, 191, 53, 59, 115, 56, 207, 215, 148, 207, 41, 130, 248, 214, 42, 124, 12, 153, 108, 197, 99, 199, 34, 66, 143, 126, 168, 88, 184, 245, 234, 37, 181, 198, 201, 84, 2];

        let mut result: u32 = 57;

        checklt238(&three238_minus1, &mut result);
        assert_eq!(result, 0, "\nExpected 0, got {}", result);

        checklt238(&three238, &mut result);
        assert_ne!(result, 0, "\nExpected nonzero, got {}", result);

        checklt238(&three238_plus1, &mut result);
        assert_ne!(result, 0, "\nExpected nonzero, got {}", result);
    }

    #[test]
    fn ephemeral_shared_secret() {
        fn shared_secrets_match(alice_secret: SIDHSecretKeyAlice, bob_secret: SIDHSecretKeyBob) -> bool {
            let start = SystemTime::now();
            let alice_public = alice_secret.public_key();
            println!("test and bench: generate alice_public from alice_secret: {}ms", SystemTime::now().duration_since(start).unwrap().as_secs_f64() * 1000.0);
            let start = SystemTime::now();
            let bob_public = bob_secret.public_key();
            println!("test and bench: generate bob_public from bob_secret: {}ms", SystemTime::now().duration_since(start).unwrap().as_secs_f64() * 1000.0);

            let start = SystemTime::now();
            let alice_shared_secret = alice_secret.shared_secret(&bob_public);
            println!("test and bench: generate alice_shared_secret: {}ms", SystemTime::now().duration_since(start).unwrap().as_secs_f64() * 1000.0);
            let start = SystemTime::now();
            let bob_shared_secret = bob_secret.shared_secret(&alice_public);
            println!("test and bench: generate bob_shared_secret: {}ms", SystemTime::now().duration_since(start).unwrap().as_secs_f64() * 1000.0);

            alice_shared_secret.iter().zip(bob_shared_secret.iter()).all(|(a, b)| a == b)
        }
        QuickCheck::new().max_tests(8).quickcheck(shared_secrets_match as fn(SIDHSecretKeyAlice, SIDHSecretKeyBob) -> bool);
    }

    #[allow(non_snake_case)]
    #[test]
    fn alice_keygen_fast_vs_slow() {
        // m_A = 2*randint(0,2^371)
        let m_A: [u8; 48] = [248, 31, 9, 39, 165, 125, 79, 135, 70, 97, 87, 231, 221, 204, 245, 38, 150, 198, 187, 184, 199, 148, 156, 18, 137, 71, 248, 83, 111, 170, 138, 61, 112, 25, 188, 197, 132, 151, 1, 0, 207, 178, 24, 72, 171, 22, 11, 0];

        let alice_secret_key = SIDHSecretKeyAlice{ scalar: m_A };
        let fast_pubkey = alice_secret_key.public_key();
        let slow_pubkey = alice_keygen_slow(&alice_secret_key);

        assert!(fast_pubkey.affine_xP.vartime_eq(&slow_pubkey.affine_xP), 
                "\nExpected affine_xP = {:?}\nfound {:?}", fast_pubkey.affine_xP, slow_pubkey.affine_xP);
        assert!(fast_pubkey.affine_xQ.vartime_eq(&slow_pubkey.affine_xQ), 
                "\nExpected affine_xQ = {:?}\nfound {:?}", fast_pubkey.affine_xQ, slow_pubkey.affine_xQ);
        assert!(fast_pubkey.affine_xQmP.vartime_eq(&slow_pubkey.affine_xQmP), 
                "\nExpected affine_xQmP = {:?}\nfound {:?}", fast_pubkey.affine_xQmP, slow_pubkey.affine_xQmP);
    }

    #[allow(non_snake_case)]
    #[test]
    fn bob_keygen_fast_vs_slow() {
        // m_B = 3*randint(0,3^238)
        let m_B: [u8; 48] = [246, 217, 158, 190, 100, 227, 224, 181, 171, 32, 120, 72, 92, 115, 113, 62, 103, 57, 71, 252, 166, 121, 126, 201, 55, 99, 213, 234, 243, 228, 171, 68, 9, 239, 214, 37, 255, 242, 217, 180, 25, 54, 242, 61, 101, 245, 78, 0];

        let bob_secret_key = SIDHSecretKeyBob{ scalar: m_B };
        let fast_pubkey = bob_secret_key.public_key();
        let slow_pubkey = bob_keygen_slow(&bob_secret_key);

        assert!(fast_pubkey.affine_xP.vartime_eq(&slow_pubkey.affine_xP), 
                "\nExpected affine_xP = {:?}\nfound {:?}", fast_pubkey.affine_xP, slow_pubkey.affine_xP);
        assert!(fast_pubkey.affine_xQ.vartime_eq(&slow_pubkey.affine_xQ), 
                "\nExpected affine_xQ = {:?}\nfound {:?}", fast_pubkey.affine_xQ, slow_pubkey.affine_xQ);
        assert!(fast_pubkey.affine_xQmP.vartime_eq(&slow_pubkey.affine_xQmP), 
                "\nExpected affine_xQmP = {:?}\nfound {:?}", fast_pubkey.affine_xQmP, slow_pubkey.affine_xQmP);
    }

    #[allow(non_snake_case)]
    #[test]
    fn shared_secret() {
        // m_A = 2*randint(0,2^371)
        let m_A: [u8; 48] = [248, 31, 9, 39, 165, 125, 79, 135, 70, 97, 87, 231, 221, 204, 245, 38, 150, 198, 187, 184, 199, 148, 156, 18, 137, 71, 248, 83, 111, 170, 138, 61, 112, 25, 188, 197, 132, 151, 1, 0, 207, 178, 24, 72, 171, 22, 11, 0];
        // m_B = 3*randint(0,3^238)
        let m_B: [u8; 48] = [246, 217, 158, 190, 100, 227, 224, 181, 171, 32, 120, 72, 92, 115, 113, 62, 103, 57, 71, 252, 166, 121, 126, 201, 55, 99, 213, 234, 243, 228, 171, 68, 9, 239, 214, 37, 255, 242, 217, 180, 25, 54, 242, 61, 101, 245, 78, 0];

        let alice_secret = SIDHSecretKeyAlice{ scalar: m_A };
        let bob_secret = SIDHSecretKeyBob{ scalar: m_B };

        let alice_public = alice_secret.public_key();
        let bob_public = bob_secret.public_key();

        let alice_shared_secret_slow = alice_shared_secret_slow(&bob_public, &alice_secret);
        let alice_shared_secret_fast = alice_secret.shared_secret(&bob_public);
        let bob_shared_secret_slow = bob_shared_secret_slow(&alice_public, &bob_secret);
        let bob_shared_secret_fast = bob_secret.shared_secret(&alice_public);

        assert!(alice_shared_secret_fast.iter().zip(bob_shared_secret_fast.iter()).all(|(a, b)| a == b), 
            "\nShared secret (fast) mismatch: Alice has {:?}\nBob has {:?}", &alice_shared_secret_fast[..], &bob_shared_secret_fast[..]);
        assert!(alice_shared_secret_slow.iter().zip(bob_shared_secret_slow.iter()).all(|(a, b)| a == b), 
            "\nShared secret (slow) mismatch: Alice has {:?}\nBob has {:?}", &alice_shared_secret_slow[..], &bob_shared_secret_slow[..]);
        assert!(alice_shared_secret_slow.iter().zip(bob_shared_secret_fast.iter()).all(|(a, b)| a == b), 
            "\nShared secret mismatch: Alice (slow) has {:?}\nBob (fast) has {:?}", &alice_shared_secret_slow[..], &bob_shared_secret_fast[..]);
    }

    #[allow(non_snake_case)]
    #[test]
    fn secret_point() {
        // m_A = 2*randint(0,2^371)
        let m_A: [u8; 48] = [248, 31, 9, 39, 165, 125, 79, 135, 70, 97, 87, 231, 221, 204, 245, 38, 150, 198, 187, 184, 199, 148, 156, 18, 137, 71, 248, 83, 111, 170, 138, 61, 112, 25, 188, 197, 132, 151, 1, 0, 207, 178, 24, 72, 171, 22, 11, 0];
        // m_B = 3*randint(0,3^238)
        let m_B: [u8; 48] = [246, 217, 158, 190, 100, 227, 224, 181, 171, 32, 120, 72, 92, 115, 113, 62, 103, 57, 71, 252, 166, 121, 126, 201, 55, 99, 213, 234, 243, 228, 171, 68, 9, 239, 214, 37, 255, 242, 217, 180, 25, 54, 242, 61, 101, 245, 78, 0];

        let xR_A = ProjectivePoint::secret_point(&AFFINE_X_PA, &AFFINE_Y_PA, &m_A[..]);
        let xR_B = ProjectivePoint::secret_point(&AFFINE_X_PB, &AFFINE_Y_PB, &m_B[..]);

        let sage_affine_xR_A = ExtensionFieldElement{
            A: Fp751Element([0x2103d089, 0x29f1dff1, 0x955e0d87, 0x7409b9bf, 0x1cca7288, 0xe812441c, 0xefba55f9, 0xc32b8b13, 0x696d83da, 0xc3b76a80, 0x3a3dc373, 0x185dd4f9, 0x115b6717, 0xfc07c1a9, 0x3b5c4254, 0x39bfcdd6, 0x1d41efd8, 0xc4d097d5, 0x389b21c7, 0x4f893494, 0x1d3d0446, 0x37343321, 0x5ccc3d22, 0x53c3]), 
            B: Fp751Element([0x33e40815, 0x722e718f, 0xdf715667, 0x8c5fc0f, 0xbbe8c74c, 0x850fd292, 0xfcbf5d3, 0x212938a6, 0xd58dc6e7, 0xfdb2a099, 0x63c9c205, 0x232f83ab, 0xa5543f5e, 0x23eda62f, 0x55d9d04f, 0x49b57588, 0x42ef25d1, 0x6b455e66, 0x37470202, 0x96511625, 0xf2e96ff0, 0xfeced582, 0xe0c0dea8, 0x33a9]) 
        };
        
        let sage_affine_xR_B = ExtensionFieldElement{
            A: Fp751Element([0x6e8499f5, 0xdd4e6607, 0x907519da, 0xe7efddc6, 0xb337108c, 0xe31f9955, 0x79ffc5e1, 0x8e558c54, 0xd776bfc2, 0xfee963ea, 0x5846bf15, 0x33aa04c3, 0x23617a0d, 0xab77d91b, 0x746070e2, 0xbdd70948, 0xc277e942, 0x66f71291, 0x2f901fce, 0x187c39db, 0xd5d32aa2, 0x69262987, 0xb40057dc, 0xe1d]), 
            B: Fp751Element([0xcfd5c167, 0xd1b766ab, 0xc8a382fa, 0x4591059d, 0x736c223d, 0x1ddf9490, 0xbdf2b3dd, 0xc96db091, 0xc292f502, 0x7b8b9c3d, 0x5e4d3e33, 0xe5b18ad8, 0x6664b931, 0xc3f3479b, 0x299e21e6, 0xa4f17865, 0x32fa1c6e, 0x3f7ef5b3, 0xdab06119, 0x875bedb5, 0xa2e23b93, 0x9b5a06e, 0x8296fb26, 0x43d4]) 
        };

        let affine_xR_A = xR_A.to_affine();
        assert!(sage_affine_xR_A.vartime_eq(&affine_xR_A), 
                "\nExpected\n{:?}\nfound\n{:?}", sage_affine_xR_A, affine_xR_A);

        let affine_xR_B = xR_B.to_affine();
        assert!(sage_affine_xR_B.vartime_eq(&affine_xR_B), 
                "\nExpected\n{:?}\nfound\n{:?}", sage_affine_xR_B, affine_xR_B);
    }
}