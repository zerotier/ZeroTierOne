// This file is part of sidh-rs.
// Copyright (c) 2017 Erkan Tairi
// See LICENSE for licensing information.
//
// Author:
// - Erkan Tairi <erkan.tairi@gmail.com>
//

//! This module contains internal curve representation and operations 
//! for SIDH, which is not part of the public API.

use crate::sidhp751::fp::Fp751Element;
use crate::sidhp751::field::{PrimeFieldElement, ExtensionFieldElement};
use crate::sidhp751::constants::*;

use std::fmt::Debug;
use std::ops::Neg;
use subtle::{ConditionallySelectable, Choice};

#[cfg(test)]
use quickcheck::{Gen, Arbitrary};
use std::mem::zeroed;

// Macro to assign tuples, as Rust does not allow tuples as lvalue.
macro_rules! assign{
    {($v1:ident, $v2:ident) = $e:expr} =>
    {
        {
            let (v1, v2) = $e;
            $v1 = v1;
            $v2 = v2;
        }
    };
}

// = 256
const CONST_256: ExtensionFieldElement = ExtensionFieldElement {
    A: Fp751Element([0x249ad67, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7300000, 0x9973da8b, 0x73815496, 0x46718c7f, 0x856657c1, 0xe363a697, 0x461860e4,0xbba838cd, 0xf9fd6510,0x06993c0c, 0x4e1a3c3f, 0xef5b75c7, 0x55ab]),
    B: Fp751Element([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
};

/// A point on the projective line `P^1(F_{p^2})`.
///
/// This is used to work projectively with the curve coefficients.
#[allow(non_snake_case)]
#[derive(Copy, Clone, PartialEq)]
pub struct ProjectiveCurveParameters {
    pub A: ExtensionFieldElement,
    pub C: ExtensionFieldElement,
}

#[allow(non_snake_case)]
struct CachedCurveParameters {
    Aplus2C: ExtensionFieldElement,
    C4: ExtensionFieldElement,
}

#[allow(non_snake_case)]
struct CachedTripleCurveParameters {
    Aminus2C: ExtensionFieldElement,
    C2: ExtensionFieldElement,
}

impl Debug for ProjectiveCurveParameters {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "ProjectiveCurveParameters(A: {:?}\nC: {:?})", &self.A, &self.C)
    }
}

#[cfg(test)]
impl Arbitrary for ProjectiveCurveParameters {
    fn arbitrary(g: &mut Gen) -> ProjectiveCurveParameters {
        let a = ExtensionFieldElement::arbitrary(g);
        let c = ExtensionFieldElement::arbitrary(g);
        ProjectiveCurveParameters{ A: a, C: c }
    }
}

impl ProjectiveCurveParameters {
    pub fn from_affine(a: &ExtensionFieldElement) -> ProjectiveCurveParameters {
        ProjectiveCurveParameters{
            A: *a,
            C: ExtensionFieldElement::one()
        }
    }

    /// Recover the curve parameters from three points on the curve.
    #[allow(non_snake_case)]
    pub fn recover_curve_parameters(affine_xP: &ExtensionFieldElement, affine_xQ: &ExtensionFieldElement, affine_xQmP: &ExtensionFieldElement) ->
                                ProjectiveCurveParameters 
    {
        let mut t0 = ExtensionFieldElement::one(); // = 1
        let mut t1 = affine_xP * affine_xQ;        // = x_P * x_Q
        t0 = &t0 - &t1;                            // = 1 - x_P * x_Q
        t1 = affine_xP * affine_xQmP;              // = x_P * x_{Q-P}
        t0 = &t0 - &t1;                            // = 1 - x_P * x_Q - x_P * x_{Q-P}
        t1 = affine_xQ * affine_xQmP;              // = x_Q * x_{Q-P}
        t0 = &t0 - &t1;                            // = 1 - x_P * x_Q - x_P * x_{Q-P} - x_Q * x_{Q-P}
        let mut a = t0.square();                   // = (1 - x_P * x_Q - x_P * x_{Q-P} - x_Q * x_{Q-P})^2
        t1 = &t1 * affine_xP;                      // = x_P * x_Q * x_{Q-P}
        t1 = &t1 + &t1;                            // = 2 * x_P * x_Q * x_{Q-P}
        let c = &t1 + &t1;                         // = 4 * x_P * x_Q * x_{Q-P}
        t0 = affine_xP + affine_xQ;                // = x_P + x_Q
        t0 = &t0 + affine_xQmP;                    // = x_P + x_Q + x_{Q-P}
        t1 = &c * &t0;                             // = 4 * x_P * x_Q * x_{Q-P} * (x_P + x_Q + x_{Q-P})
        a = &a - &t1;                              // = (1 - x_P * x_Q - x_P * x_{Q-P} - x_Q * x_{Q-P})^2 - 4 * x_P * x_Q * x_{Q-P} * (x_P + x_Q + x_{Q-P})
        
        ProjectiveCurveParameters{ A: a, C: c }
    }

    /// Compute the j-invariant of the given curve.
    pub fn j_invariant(&self) -> ExtensionFieldElement {
        let a = &self.A;
        let c = &self.C;
        let mut v0 = c.square();    // C^2
        let mut v1 = a.square();    // A^2
        let mut v2 = &v0 + &v0;     // 2C^2
        let mut v3 = &v2 + &v0;     // 3C^2
        v2 = &v2 + &v2;             // 4C^2
        v2 = &v1 - &v2;             // A^2 - 4C^2
        v1 = &v1 - &v3;             // A^2 - 3C^2
        v3 = v1.square();           // (A^2 - 3C^2)^2
        v3 = &v3 * &v1;             // (A^2 - 3C^2)^3
        v0 = v0.square();           // C^4
        v3 = &v3 * &CONST_256;      // 256(A^2 - 3C^2)^3
        v2 = &v2 * &v0;             // C^4(A^2 - 4C^2)
        v2 = v2.inv();              // 1/C^4(A^2 - 4C^2)
        v0 = &v3 * &v2;             // 256(A^2 - 3C^2)^3 / C^4(A^2 - 4C^2)

        v0
    }

    /// Compute cached parameters `A + 2C, 4C`.
    #[allow(non_snake_case)]
    fn cached_params(&self) -> CachedCurveParameters {
        let mut Aplus2C = &self.C + &self.C; // = 2*C
        let C4 = &Aplus2C + &Aplus2C;        // = 4*C
        Aplus2C = &Aplus2C + &self.A;        // = 2*C + A

        CachedCurveParameters{ Aplus2C, C4 }
    }

    /// Compute cached parameters `A - 2C, 2C`.
    #[allow(non_snake_case)]
    fn cached_triple_params(&self) -> CachedTripleCurveParameters {
        let C2 = &self.C + &self.C;   // = 2*C
        let Aminus2C = &self.A - &C2; // = A -2*C

        CachedTripleCurveParameters{ Aminus2C, C2 }
    }
}

/// A point on the projective line `P^1(F_{p^2})`.
///
/// This represents a point on the (Kummer line) of a Montgomery curve.  The
/// curve is specified by a ProjectiveCurveParameters struct.
#[allow(non_snake_case)]
#[derive(Copy, Clone, PartialEq)]
pub struct ProjectivePoint {
    pub X: ExtensionFieldElement,
    pub Z: ExtensionFieldElement,
}

impl ConditionallySelectable for ProjectivePoint {
    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        ProjectivePoint{
            X: ExtensionFieldElement::conditional_select(&a.X, &b.X, choice),
            Z: ExtensionFieldElement::conditional_select(&a.Z, &b.Z, choice)
        }
    }

    fn conditional_swap(a: &mut Self, b: &mut Self, choice: Choice) {
        ExtensionFieldElement::conditional_swap(&mut a.X, &mut b.X, choice);
        ExtensionFieldElement::conditional_swap(&mut a.Z, &mut b.Z, choice);
    }
}

impl Debug for ProjectivePoint {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "ProjectivePoint(X: {:?}\nZ: {:?})", &self.X, &self.Z)
    }
}

#[cfg(test)]
impl Arbitrary for ProjectivePoint {
    fn arbitrary(g: &mut Gen) -> ProjectivePoint {
        let x = ExtensionFieldElement::arbitrary(g);
        let z = ExtensionFieldElement::arbitrary(g);
        ProjectivePoint{ X: x, Z: z }
    }
}

impl ProjectivePoint {
    /// Creates a new zero `ProejctivePoint`.
    pub fn new() -> ProjectivePoint {
        unsafe { zeroed() }
        //ProjectivePoint{ X: ExtensionFieldElement::zero(), Z: ExtensionFieldElement::zero() }
    }

    #[allow(non_snake_case)]
    pub fn from_affine_prime_field(x: &PrimeFieldElement) -> ProjectivePoint {
        let _X = ExtensionFieldElement{ A: x.A, B: ExtensionFieldElement::zero().B };
        ProjectivePoint{
            X: _X,
            Z: ExtensionFieldElement::one()
        }
    }

    pub fn from_affine(x: &ExtensionFieldElement) -> ProjectivePoint {
        ProjectivePoint{
            X: *x,
            Z: ExtensionFieldElement::one()
        }
    }

    pub fn to_affine(&self) -> ExtensionFieldElement {
        let affine_x = &self.Z.inv() * &self.X;
        affine_x
    }

    /// Returns true if both sides are equal. Takes variable time.
    pub fn vartime_eq(&self, _rhs: &ProjectivePoint) -> bool {
        let t0 = &self.X * &_rhs.Z;
        let t1 = &self.Z * &_rhs.X;
        t0.vartime_eq(&t1)
    }

    /// Given `xP = x(P), xQ = x(Q)`, and `xPmQ = x(P-Q)`, compute `xR = x(P+Q)`.
    #[allow(non_snake_case)]
    fn add(&self, xQ: &ProjectivePoint, xPmQ: &ProjectivePoint) -> ProjectivePoint {
        let xP = *self;
        // Algorithm 1 of Costello-Smith.
        let mut v0 = &xP.X + &xP.Z;         // X_P + Z_P
        let v1 = &(&xQ.X - &xQ.Z) * &v0;    // (X_Q - Z_Q)(X_P + Z_P)
        v0 = &xP.X - &xP.Z;                 // X_P - Z_P
        let v2 = &(&xQ.X + &xQ.Z) * &v0;    // (X_Q + Z_Q)(X_P - Z_P)
        let v3 = (&v1 + &v2).square();      // 4(X_Q X_P - Z_Q Z_P)^2
        let v4 = (&v1 - &v2).square();      // 4(X_Q Z_P - Z_Q X_P)^2
        v0 = &xPmQ.Z * &v3;                 // 4X_{P-Q}(X_Q X_P - Z_Q Z_P)^2
        let z = &xPmQ.X * &v4;              // 4Z_{P-Q}(X_Q Z_P - Z_Q X_P)^2
        let x = v0;

        ProjectivePoint{ X: x, Z: z }
    }

    /// Given `xP = x(P)` and cached curve parameters `Aplus2C = A + 2*C, C4 = 4*C`, compute `xQ = x([2]P)`.
    #[allow(non_snake_case)]
    fn double(&self, curve: &CachedCurveParameters) -> ProjectivePoint {
        let xP = *self;
        // Algorithm 2 of Costello-Smith, amended to work with projective curve coefficients.
        let v1 = (&xP.X + &xP.Z).square();      // (X+Z)^2
        let mut v2 = (&xP.X - &xP.Z).square();  // (X-Z)^2
        let xz4 = &v1 - &v2;                    // 4XZ = (X+Z)^2 - (X-Z)^2
        v2 = &v2 * &curve.C4;                   // 4C(X-Z)^2
        let x = &v1 * &v2;                      // 4C(X+Z)^2(X-Z)^2
        let mut v3 = &xz4 * &curve.Aplus2C;     // 4XZ(A + 2C)
        v3 = &v3 + &v2;                         // 4XZ(A + 2C) + 4C(X-Z)^2
        let z = &v3 * &xz4;                     // (4XZ(A + 2C) + 4C(X-Z)^2)4XZ
        // Now (xQ.x : xQ.z)
        //   = (4C(X+Z)^2(X-Z)^2 : (4XZ(A + 2C) + 4C(X-Z)^2)4XZ )
        //   = ((X+Z)^2(X-Z)^2 : (4XZ((A + 2C)/4C) + (X-Z)^2)4XZ )
        //   = ((X+Z)^2(X-Z)^2 : (4XZ((a + 2)/4) + (X-Z)^2)4XZ )
        ProjectivePoint{ X: x, Z: z }
    }

    /// Calculates the x-coordinate of `2P` and `P+Q` from the x-coordinate of `P, Q` and `P-Q`.
    // Params: `C4 = 4*C` and `Aplus2C = (A+2C)`
    // Cost: 8M+4S+8A in `Fp2`
    #[allow(non_snake_case)]
    fn dbl_add(&self, xQ: &ProjectivePoint, xPmQ: &ProjectivePoint, params: &CachedCurveParameters) ->
              (ProjectivePoint, ProjectivePoint)
    {
        let xP = *self;
        let (x1, z1) = (&xPmQ.X, &xPmQ.Z);
        let (x2, z2) = (&xP.X, &xP.Z);
        let (x3, z3) = (&xQ.X, &xQ.Z);

        let mut t0 = x2 + z2;   // A = x2+z2
        let mut t1 = x2 - z2;   // B = x2-z2
        let mut t3 = x3 + z3;   // C = x3+z3
        let mut t2 = x3 - z3;   // D = x3-z3
        t2 = &t2 * &t0;         // DA = D*A
        t3 = &t3 * &t1;         // CB = C*B

        let mut x = &t2 + &t3;  // x5 = DA+CB
        let mut z = &t2 - &t3;  // z5 = DA-CB
        x = x.square();         // x5 = (DA+CB)^2
        z = z.square();         // z5 = (DA-CB)^2
        x = &x * z1;            // x5 = z1*(DA+CB)^2
        z = &z * x1;            // z5 = x1*(DA-CB)^2
        let xPaddQ = ProjectivePoint{ X: x, Z: z };

        t0 = t0.square();           // t0 = AA = A^2
        t1 = t1.square();           // t1 = BB = B^2
        t2 = &t0 - &t1;             // t2 = E = AA-BB
        t3 = &t1 * &params.C4;      // t3 = (4C)*BB
        z = &t2 * &params.Aplus2C;  // z4 = (A+2C)*E
        z = &z + &t3;               // z4 = (4C)*BB+(A+2C)*E
        x = &t0 * &t3;              // x4 = AA*(4C)*BB
        z = &z * &t2;               // z4 = E*((4C)*BB+(A+2C)*E)
        let x2P = ProjectivePoint{ X: x, Z: z };

        (x2P, xPaddQ)
    }

    /// Given the curve parameters, `xP = x(P)`, and `k >= 0`, compute `xQ = x([2^k]P)`.
    #[allow(non_snake_case)]
    pub fn pow2k(&self, curve: &ProjectiveCurveParameters, k: u32) -> ProjectivePoint {
        let cached_params = curve.cached_params();
        let mut xQ = *self;
        for _ in 0..k { xQ = xQ.double(&cached_params); }
        xQ
    }

    // Uses the efficient Montgomery tripling formulas from FLOR-SIDH-x64
    // Reference: A faster SW implementation of SIDH (github.com/armfazh/flor-sidh-x64).
    /// Given `xP = x(P)` and cached tripling curve parameters `Aminus2C = A - 2*C, C2 = 2*C`, compute `xQ = x([3]P)`.
    /// Returns `xQ` to allow chaining.
    #[allow(non_snake_case)]
    fn triple(&self, curve: &CachedTripleCurveParameters) -> ProjectivePoint {
        let xP = *self;
        let (x1, z1) = (&xP.X, &xP.Z);
        let mut t0 = x1.square();           // t0 = x1^2
        let mut t1 = z1.square();           // t1 = z1^2
        let mut t2 = x1 + z1;               // t2 = x1+z1
        t2 = t2.square();                   // t2 = t2^2
        let t3 = &t0 + &t1;                 // t3 = t0+t1
        let mut t4 = &t2 - &t3;             // t4 = t2-t3
        let mut t5 = &curve.Aminus2C * &t4; // t5 = (A-2C)*t4
        t2 = &curve.C2 * &t2;               // t2 = (2C)*t2
        t5 = &t5 + &t2;                     // t5 = t2+t5
        t5 = &t5 + &t5;                     // t5 = t5+t5
        t5 = &t5 + &t5;                     // t5 = t5+t5
        t0 = &t0 * &t5;                     // t0 = t0*t5
        t1 = &t1 * &t5;                     // t1 = t1*t5
        t4 = &t3 - &t4;                     // t4 = t3-t4
        t2 = &t2 * &t4;                     // t2 = t2*t4
        t0 = &t2 - &t0;                     // t0 = t2-t0
        t1 = &t2 - &t1;                     // t1 = t2-t1
        t0 = t0.square();                   // t0 = t0^2
        t1 = t1.square();                   // t1 = t1^2
        let x = x1 * &t1;                   // x3 = x1*t1
        let z = z1 * &t0;                   // z3 = z1*t0

        ProjectivePoint{ X: x, Z: z }
    }

    /// Given the curve parameters, `xP = x(P)`, and `k >= 0`, compute `xQ = x([3^k]P)`.
    #[allow(non_snake_case)]
    pub fn pow3k(&self, curve: &ProjectiveCurveParameters, k: u32) -> ProjectivePoint {
        let cached_params = curve.cached_triple_params();
        let mut xQ = *self;
        for _ in 0..k { xQ = xQ.triple(&cached_params); }
        xQ
    }

    /// Given `x(P)` and a scalar `m` in little-endian bytes, compute `x([m]P)` using the
    /// Montgomery ladder. This is described in Algorithm 8 of Costello-Smith.
    ///
    /// This function's execution time is dependent only on the byte-length of the
    /// input scalar. All scalars of the same input length execute in uniform time.
    /// The scalar can be padded with zero bytes to ensure a uniform length.
    #[allow(non_snake_case)]
    fn scalar_mul(&self, curve: &ProjectiveCurveParameters, scalar: &[u8]) -> ProjectivePoint {
        let xP = *self;
        let cached_params = curve.cached_params();
        let mut x0 = ProjectivePoint{ X: ExtensionFieldElement::one(), Z: ExtensionFieldElement::zero() };
        let mut x1 = xP;
        let mut tmp: ProjectivePoint;

        // Iterate over the bits of the scalar, top to bottom.
        let mut prev_bit: u8 = 0;
        for i in (0..scalar.len()).rev() {
            let scalar_byte = scalar[i];
            for j in (0..8).rev() {
                let bit = (scalar_byte >> (j as u32)) & 0x1;
                ProjectivePoint::conditional_swap(&mut x0, &mut x1, (bit ^ prev_bit).into());
                tmp = x0.double(&cached_params);
                x1 = x0.add(&x1, &xP);
                x0 = tmp;
                prev_bit = bit;
            }
        }
        // Now prev_bit is the lowest bit of the scalar.
        ProjectivePoint::conditional_swap(&mut x0, &mut x1, prev_bit.into());
        let xQ = x0;
        xQ
    }

    /// Given `P = (x_P, y_P)` in affine coordinates, as well as projective points
    /// `x(Q), x(R) = x(P+Q)`, all in the prime-field subgroup of the starting curve
    /// `E_0(F_p)`, use the Okeya-Sakurai coordinate recovery strategy to recover `Q =
    /// (X_Q : Y_Q : Z_Q)`.
    ///
    /// This is Algorithm 5 of Costello-Smith, with the constants `a = 0, b = 1` hardcoded.
    #[allow(non_snake_case)]
    fn okeya_sakurai_coordinate_recovery(affine_xP: &PrimeFieldElement, affine_yP: &PrimeFieldElement,
                                         xQ: &ProjectivePrimeFieldPoint, xR: &ProjectivePrimeFieldPoint) ->
                                        (PrimeFieldElement, PrimeFieldElement, PrimeFieldElement)
    {
        let mut v1 = affine_xP * &xQ.Z;      // = x_P*Z_Q
        let mut v2 = &xQ.X + &v1;            // = X_Q + x_P*Z_Q
        let mut v3 = (&xQ.X - &v1).square(); // = (X_Q - x_P*Z_Q)^2
        v3 = &v3 * &xR.X;                    // = X_R*(X_Q - x_P*Z_Q)^2
        // Skip setting v1 = 2a*Z_Q (step 6) since we hardcode a = 0.
	    // Skip adding v1 to v2 (step 7) since v1 is zero.
        let mut v4 = affine_xP * &xQ.X; // = x_P*X_Q
        v4 = &v4 + &xQ.Z;               // = x_P*X_Q + Z_Q
        v2 = &v2 * &v4;                 // = (x_P*X_Q + Z_Q)*(X_Q + x_P*Z_Q)
        // Skip multiplication by v1 (step 11) since v1 is zero.
	    // Skip subtracting v1 from v2 (step 12) since v1 is zero.
        v2 = &v2 * &xR.Z;               // = (x_P*X_Q + Z_Q)*(X_Q + x_P*Z_Q)*Z_R
        let Y_Q = &v2 - &v3;            // = (x_P*X_Q + Z_Q)*(X_Q + x_P*Z_Q)*Z_R - X_R*(X_Q - x_P*Z_Q)^2
        v1 = affine_yP + affine_yP;     // = 2b*y_P
        v1 = &(&v1 * &xQ.Z) * &xR.Z;    // = 2b*y_P*Z_Q*Z_R
        let X_Q = &v1 * &xQ.X;          // = 2b*y_P*Z_Q*Z_R*X_Q
        let Z_Q = &v1 * &xQ.Z;          // = 2b*y_P*Z_Q^2*Z_R

        (X_Q, Y_Q, Z_Q)
    }

    /// Given `x(P), x(Q), x(P-Q)`, as well as a scalar m in little-endian bytes,
    /// compute `x(P + [m]Q)` using the "three-point ladder" of de Feo, Jao, and Plut.
    ///
    /// This function's execution time is dependent only on the byte-length of the
    /// input scalar. All scalars of the same input length execute in uniform time.
    /// The scalar can be padded with zero bytes to ensure a uniform length.
    //
    // The algorithm, as described in de Feo-Jao-Plut, is as follows:
    //
    // (x0, x1, x2) <--- (x(O), x(Q), x(P))
    //
    // for i = |m| down to 0, indexing the bits of m:
    //     Invariant: (x0, x1, x2) == (x( [t]Q ), x( [t+1]Q ), x( P + [t]Q ))
    //          where t = m//2^i is the high bits of m, starting at i
    //     if m_i == 0:
    //         (x0, x1, x2) <--- (xDBL(x0), xADD(x1, x0, x(Q)), xADD(x2, x0, x(P)))
    //         Invariant: (x0, x1, x2) == (x( [2t]Q ), x( [2t+1]Q ), x( P + [2t]Q ))
    //                                 == (x( [t']Q ), x( [t'+1]Q ), x( P + [t']Q ))
    //              where t' = m//2^{i-1} is the high bits of m, starting at i-1
    //     if m_i == 1:
    //         (x0, x1, x2) <--- (xADD(x1, x0, x(Q)), xDBL(x1), xADD(x2, x1, x(P-Q)))
    //         Invariant: (x0, x1, x2) == (x( [2t+1]Q ), x( [2t+2]Q ), x( P + [2t+1]Q ))
    //                                 == (x( [t']Q ),   x( [t'+1]Q ), x( P + [t']Q ))
    //              where t' = m//2^{i-1} is the high bits of m, starting at i-1
    // return x2
    //
    // Notice that the roles of (x0,x1) and (x(P), x(P-Q)) swap depending on the
    // current bit of the scalar.  Instead of swapping which operations we do, we
    // can swap variable names, producing the following uniform algorithm:
    //
    // (x0, x1, x2) <--- (x(O), x(Q), x(P))
    // (y0, y1) <--- (x(P), x(P-Q))
    //
    // for i = |m| down to 0, indexing the bits of m:
    //      (x0, x1) <--- SWAP( m_{i+1} xor m_i, (x0,x1) )
    //      (y0, y1) <--- SWAP( m_{i+1} xor m_i, (y0,y1) )
    //      (x0, x1, x2) <--- ( xDBL(x0), xADD(x1,x0,x(Q)), xADD(x2, x0, y0) )
    //
    // return x2
    //
    #[allow(non_snake_case)]
    pub fn three_point_ladder(xP: &ProjectivePoint, xQ: &ProjectivePoint, xPmQ: &ProjectivePoint,
                              curve: &ProjectiveCurveParameters, scalar: &[u8]) -> ProjectivePoint
    {
        let cached_params = curve.cached_params();

        // (x0, x1, x2) <--- (x(O), x(Q), x(P))
        let mut x0 = ProjectivePoint{ X: ExtensionFieldElement::one(), Z: ExtensionFieldElement::zero() };
        let mut x1 = *xQ;
        let mut x2 = *xP;
        // (y0, y1) <--- (x(P), x(P-Q))
        let mut y0 = *xP;
        let mut y1 = *xPmQ;

        // Iterate over the bits of the scalar, top to bottom.
        let mut prev_bit: u8 = 0;
        for i in (0..scalar.len()).rev() {
            let scalar_byte = scalar[i];
            for j in (0..8).rev() {
                let bit = (scalar_byte >> (j as u32)) & 0x1;
                ProjectivePoint::conditional_swap(&mut x0, &mut x1, (bit ^ prev_bit).into());
                ProjectivePoint::conditional_swap(&mut y0, &mut y1, (bit ^ prev_bit).into());
                x1 = x1.add(&x0, xQ); // = xADD(x1, x0, x(Q))
                assign!{(x0, x2) = x0.dbl_add(&x2, &y0, &cached_params)};
                prev_bit = bit;
            }
        }

        let xR = x2;
        xR
    }

    /// Right-to-left point multiplication, which given the x-coordinate
    /// of `P, Q` and `P-Q` calculates the x-coordinate of `R=P+[k]Q`.
    #[allow(non_snake_case)]
    pub fn right_to_left_ladder(xP: &ProjectivePoint, xQ: &ProjectivePoint, xPmQ: &ProjectivePoint,
                                curve: &ProjectiveCurveParameters, scalar: &[u8]) -> ProjectivePoint
    {
        let cached_params = curve.cached_params();
        let mut R1 = *xP;
        let mut R2 = *xPmQ;
        let mut R0 = *xQ;

        // Iterate over the bits of the scalar, bottom to top.
        let mut prev_bit: u8 = 0;
        for i in 0..scalar.len() {
            let scalar_byte = scalar[i];
            for j in 0..8 {
                let bit = (scalar_byte >> (j as u32)) & 0x1;
                ProjectivePoint::conditional_swap(&mut R1, &mut R2, (bit ^ prev_bit).into());
                assign!{(R0, R2) = R0.dbl_add(&R2, &R1, &cached_params)};
                prev_bit = bit;
            }
        }
        ProjectivePoint::conditional_swap(&mut R1, &mut R2, prev_bit.into());
        let xR = R1;
        xR
    }

    /// Given the affine x-coordinate `affine_xP` of `P`, compute the x-coordinate
    /// `x(\tau(P)-P) of \tau(P)-P`.
    #[allow(non_snake_case)]
    pub fn distort_and_difference(affine_xP: &PrimeFieldElement) -> ProjectivePoint {
        let mut t0 = affine_xP.square();            // = x_P^2
        let t1 = &PrimeFieldElement::one() + &t0;   // = x_P^2 + 1
        let b = t1.A;                               // = 0 + (x_P^2 + 1)*i
        t0 = affine_xP + affine_xP;                 // = 2*x_P
        let a = t0.A;                               // = 2*x_P + 0*i

        let x = ExtensionFieldElement{ A: Fp751Element::zero(), B: b };
        let z = ExtensionFieldElement{ A: a, B: Fp751Element::zero() };
        let xR = ProjectivePoint{ X: x, Z: z };
        xR
    }

    /// Given an affine point `P = (x_P, y_P)` in the prime-field subgroup of the
    /// starting curve `E_0(F_p)`, together with a secret scalar `m`, compute `x(P+[m]Q)`,
    /// where `Q = \tau(P)` is the image of `P` under the distortion map.
    //
    // The computation uses basically the same strategy as the
    // Costello-Longa-Naehrig implementation:
    //
    // 1. Use the standard Montgomery ladder to compute x([m]Q), x([m+1]Q)
    //
    // 2. Use Okeya-Sakurai coordinate recovery to recover [m]Q from Q, x([m]Q),
    // x([m+1]Q)
    //
    // 3. Use P and [m]Q to compute x(P + [m]Q)
    //
    // The distortion map \tau is defined as
    //
    // \tau : E_0(F_{p^2}) ---> E_0(F_{p^2})
    //
    // \tau : (x,y) |---> (-x, iy).
    //
    // The image of the distortion map is the _trace-zero_ subgroup of E_0(F_{p^2})
    // defined by Tr(P) = P + \pi_p(P) = id, where \pi_p((x,y)) = (x^p, y^p) is the
    // p-power Frobenius map.  To see this, take P = (x,y) \in E_0(F_{p^2}).  Then
    // Tr(P) = id if and only if \pi_p(P) = -P, so that
    //
    // -P = (x, -y) = (x^p, y^p) = \pi_p(P);
    //
    // we have x^p = x if and only if x \in F_p, while y^p = -y if and only if y =
    // i*y' for y' \in F_p.
    //
    // Thus (excepting the identity) every point in the trace-zero subgroup is of
    // the form \tau((x,y)) = (-x,i*y) for (x,y) \in E_0(F_p).
    //
    // Since the Montgomery ladder only uses the x-coordinate, and the x-coordinate
    // is always in the prime subfield, we can compute x([m]Q), x([m+1]Q) entirely
    // in the prime subfield.
    //
    // The affine form of the relation for Okeya-Sakurai coordinate recovery is
    // given on p. 13 of Costello-Smith:
    //
    // y_Q = ((x_P*x_Q + 1)*(x_P + x_Q + 2*a) - 2*a - x_R*(x_P - x_Q)^2)/(2*b*y_P),
    //
    // where R = Q + P and a,b are the Montgomery parameters.  In our setting
    // (a,b)=(0,1) and our points are P=Q, Q=[m]Q, P+Q=[m+1]Q, so this becomes
    //
    // y_{mQ} = ((x_Q*x_{mQ} + 1)*(x_Q + x_{mQ}) - x_{m1Q}*(x_Q - x_{mQ})^2)/(2*y_Q)
    //
    // y_{mQ} = ((1 - x_P*x_{mQ})*(x_{mQ} - x_P) - x_{m1Q}*(x_P + x_{mQ})^2)/(2*y_P*i)
    //
    // y_{mQ} = i*((1 - x_P*x_{mQ})*(x_{mQ} - x_P) - x_{m1Q}*(x_P + x_{mQ})^2)/(-2*y_P)
    //
    // since (x_Q, y_Q) = (-x_P, y_P*i).  In projective coordinates this is
    //
    // Y_{mQ}' = ((Z_{mQ} - x_P*X_{mQ})*(X_{mQ} - x_P*Z_{mQ})*Z_{m1Q}
    //          - X_{m1Q}*(X_{mQ} + x_P*Z_{mQ})^2)
    //
    // with denominator
    //
    // Z_{mQ}' = (-2*y_P*Z_{mQ}*Z_{m1Q})*Z_{mQ}.
    //
    // Setting
    //
    // X_{mQ}' = (-2*y_P*Z_{mQ}*Z_{m1Q})*X_{mQ}
    //
    // gives [m]Q = (X_{mQ}' : i*Y_{mQ}' : Z_{mQ}') with X,Y,Z all in F_p.  (Here
    // the ' just denotes that we've added extra terms to the denominators during
    // the computation of Y)
    //
    // To compute the x-coordinate x(P+[m]Q) from P and [m]Q, we use the affine
    // addition formulas of section 2.2 of Costello-Smith.  We're only interested
    // in the x-coordinate, giving
    //
    // X_R = Z_{mQ}*(i*Y_{mQ} - y_P*Z_{mQ})^2 - (x_P*Z_{mQ} + X_{mQ})*(X_{mQ} - x_P*Z_{mQ})^2
    //
    // Z_R = Z_{mQ}*(X_{mQ} - x_P*Z_{mQ})^2.
    //
    // Notice that although X_R \in F_{p^2}, we can split the computation into
    // coordinates X_R = X_{R,a} + X_{R,b}*i as
    //
    // (i*Y_{mQ} - y_P*Z_{mQ})^2 = (y_P*Z_{mQ})^2 - Y_{mQ}^2 - 2*y_P*Z_{mQ}*Y_{mQ}*i,
    //
    // giving
    //
    // X_{R,a} = Z_{mQ}*((y_P*Z_{mQ})^2 - Y_{mQ}^2)
    //         - (x_P*Z_{mQ} + X_{mQ})*(X_{mQ} - x_P*Z_{mQ})^2
    //
    // X_{R,b} = -2*y_P*Y_{mQ}*Z_{mQ}^2
    //
    // Z_R = Z_{mQ}*(X_{mQ} - x_P*Z_{mQ})^2.
    //
    // These formulas could probably be combined with the formulas for y-recover
    // and computed more efficiently, but efficiency isn't the biggest concern
    // here, since the bulk of the cost is already in the ladder.
    #[allow(non_snake_case)]
    pub fn secret_point(affine_xP: &PrimeFieldElement, affine_yP: &PrimeFieldElement, scalar: &[u8]) -> ProjectivePoint {
        let mut xQ = ProjectivePrimeFieldPoint::from_affine(affine_xP);
        xQ.X = (&xQ.X).neg();

        // Compute x([m]Q) = (X_{mQ} : Z_{mQ}), x([m+1]Q) = (X_{m1Q} : Z_{m1Q}).
        let (xmQ, xm1Q) = xQ.scalar_mul_prime_field(&E0_A_PLUS2_OVER4, scalar);

        // Now perform coordinate recovery:
	    // [m]Q = (X_{mQ} : Y_{mQ}*i : Z_{mQ})

        // Y_{mQ} = (Z_{mQ} - x_P*X_{mQ})*(X_{mQ} - x_P*Z_{mQ})*Z_{m1Q}
	    //          - X_{m1Q}*(X_{mQ} + x_P*Z_{mQ})^2
        let mut t0 = affine_xP * &xmQ.X;    // = x_P*X_{mQ}
        let mut YmQ = &xmQ.Z - &t0;         // = Z_{mQ} - x_P*X_{mQ}
        let mut t1 = affine_xP * &xmQ.Z;    // = x_P*Z_{mQ}
        t0 = &xmQ.X - &t1;                  // = X_{mQ} - x_P*Z_{mQ}
        YmQ = &YmQ * &t0;                   // = (Z_{mQ} - x_P*X_{mQ})*(X_{mQ} - x_P*Z_{mQ})
        YmQ = &YmQ * &xm1Q.Z;               // = (Z_{mQ} - x_P*X_{mQ})*(X_{mQ} - x_P*Z_{mQ})*Z_{m1Q}
        t1 = (&t1 + &xmQ.X).square();       // = (X_{mQ} + x_P*Z_{mQ})^2
        t1 = &t1 * &xm1Q.X;                 // = X_{m1Q}*(X_{mQ} + x_P*Z_{mQ})^2
        YmQ = &YmQ - &t1;                   // = Y_{mQ}

        // Z_{mQ} = -2*(Z_{mQ}^2 * Z_{m1Q} * y_P)
        t0 = &(&xmQ.Z * &xm1Q.Z) * affine_yP;   // = Z_{mQ} * Z_{m1Q} * y_P
        t0 = (&t0).neg();                            // = -1*(Z_{mQ} * Z_{m1Q} * y_P)
        t0 = &t0 + &t0;                         // = -2*(Z_{mQ} * Z_{m1Q} * y_P)
        let ZmQ = &xmQ.Z * &t0;                 // = -2*(Z_{mQ}^2 * Z_{m1Q} * y_P)

        // We added terms to the denominator Z_{mQ}, so multiply them to X_{mQ}.
	    // X_{mQ} = -2*X_{mQ}*Z_{mQ}*Z_{m1Q}*y_P
        let XmQ = &xmQ.X * &t0;

        // Now compute x(P + [m]Q) = (X_Ra + i*X_Rb : Z_R)
        let mut XRb = &ZmQ.square() * &YmQ; // = Y_{mQ} * Z_{mQ}^2
        XRb = &XRb * affine_yP;             // = Y_{mQ} * y_P * Z_{mQ}^2
        XRb = &XRb + &XRb;                  // = 2 * Y_{mQ} * y_P * Z_{mQ}^2
        XRb = (&XRb).neg();                      // = -2 * Y_{mQ} * y_P * Z_{mQ}^2

        t0 = (affine_yP * &ZmQ).square();   // = (y_P * Z_{mQ})^2
        t1 = YmQ.square();                  // = Y_{mQ}^2
        let mut XRa = &t0 - &t1;            // = (y_P * Z_{mQ})^2 - Y_{mQ}^2
        XRa = &XRa * &ZmQ;                  // = Z_{mQ}*((y_P * Z_{mQ})^2 - Y_{mQ}^2)
        t0 = affine_xP * &ZmQ;              // = x_P * Z_{mQ}
        t1 = &XmQ + &t0;                    // = X_{mQ} + x_P*Z_{mQ}
        t0 = &XmQ - &t0;                    // = X_{mQ} - x_P*Z_{mQ}
        t0 = t0.square();                   // = (X_{mQ} - x_P*Z_{mQ})^2
        t1 = &t1 * &t0;                     // = (X_{mQ} + x_P*Z_{mQ})*(X_{mQ} - x_P*Z_{mQ})^2
        XRa = &XRa - &t1;                   // = Z_{mQ}*((y_P*Z_{mQ})^2 - Y_{mQ}^2) - (X_{mQ} + x_P*Z_{mQ})*(X_{mQ} - x_P*Z_{mQ})^2

        let ZR = &ZmQ * &t0;                // = Z_{mQ}*(X_{mQ} - x_P*Z_{mQ})^2

        let mut xR = ProjectivePoint{ X: ExtensionFieldElement::zero(), Z: ExtensionFieldElement::zero() };
        xR.X.A = XRa.A;
        xR.X.B = XRb.A;
        xR.Z.A = ZR.A;

        xR
    }
}

/// A point on the projective line `P^1(F_p)`.
///
/// This represents a point on the (Kummer line) of the prime-field subgroup of
/// the base curve `E_0(F_p)`, defined by `E_0 : y^2 = x^3 + x`.
#[allow(non_snake_case)]
#[derive(Copy, Clone, PartialEq)]
struct ProjectivePrimeFieldPoint {
    X: PrimeFieldElement,
    Z: PrimeFieldElement,
}

impl ConditionallySelectable for ProjectivePrimeFieldPoint {
    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        ProjectivePrimeFieldPoint{
            X: PrimeFieldElement::conditional_select(&a.X, &b.X, choice),
            Z: PrimeFieldElement::conditional_select(&a.Z, &b.Z, choice)
        }
    }

    fn conditional_swap(a: &mut Self, b: &mut Self, choice: Choice) {
        PrimeFieldElement::conditional_swap(&mut a.X, &mut b.X, choice);
        PrimeFieldElement::conditional_swap(&mut a.Z, &mut b.Z, choice);
    }
}

impl Debug for ProjectivePrimeFieldPoint {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "ProjectivePrimeFieldPoint(X: {:?}\nZ: {:?})", &self.X, &self.Z)
    }
}

#[cfg(test)]
impl Arbitrary for ProjectivePrimeFieldPoint {
    fn arbitrary(g: &mut Gen) -> ProjectivePrimeFieldPoint {
        let x = PrimeFieldElement::arbitrary(g);
        let z = PrimeFieldElement::arbitrary(g);
        ProjectivePrimeFieldPoint{ X: x, Z: z }
    }
}

impl ProjectivePrimeFieldPoint {
    /// Creates a new zero `ProjectivePrimeFieldPoint`.
    pub fn new() -> ProjectivePrimeFieldPoint {
        unsafe { zeroed() }
        //ProjectivePrimeFieldPoint{ X: PrimeFieldElement::zero(), Z: PrimeFieldElement::zero() }
    }

    pub fn from_affine(x: &PrimeFieldElement) -> ProjectivePrimeFieldPoint {
        ProjectivePrimeFieldPoint{
            X: *x,
            Z: PrimeFieldElement::one()
        }
    }

    pub fn to_affine(&self) -> PrimeFieldElement {
        &self.Z.inv() * &self.X
    }

    /// Returns true if both sides are equal. Takes variable time.
    pub fn vartime_eq(&self, _rhs: &ProjectivePrimeFieldPoint) -> bool {
        let t0 = &self.X * &_rhs.Z;
        let t1 = &self.Z * &_rhs.X;
        t0.vartime_eq(&t1)
    }

    /// Given `xP = x(P), xQ = x(Q)`, and `xPmQ = x(P-Q)`, compute `xR = x(P+Q)`.
    #[allow(non_snake_case)]
    fn add(&self, xQ: &ProjectivePrimeFieldPoint, xPmQ: &ProjectivePrimeFieldPoint) ->
           ProjectivePrimeFieldPoint
    {
        let xP = *self;
        // Algorithm 1 of Costello-Smith.
        let mut v0 = &xP.X + &xP.Z;         // X_P + Z_P
        let v1 = &(&xQ.X - &xQ.Z) * &v0;    // (X_Q - Z_Q)(X_P + Z_P)
        v0 = &xP.X - &xP.Z;                 // X_P - Z_P
        let v2 = &(&xQ.X + &xQ.Z) * &v0;    // (X_Q + Z_Q)(X_P - Z_P)
        let v3 = (&v1 + &v2).square();      // 4(X_Q X_P - Z_Q Z_P)^2
        let v4 = (&v1 - &v2).square();      // 4(X_Q Z_P - Z_Q X_P)^2
        v0 = &xPmQ.Z * &v3;                 // 4X_{P-Q}(X_Q X_P - Z_Q Z_P)^2
        let z = &xPmQ.X * &v4;              // 4Z_{P-Q}(X_Q Z_P - Z_Q X_P)^2
        let x = v0;

        ProjectivePrimeFieldPoint{ X: x, Z: z }
    }

    /// Given `xP = x(P)` and cached curve parameter `aPlus2Over4 = (a+2)/4, compute xQ = x([2]P)`.
    //
    // Note that we don't use projective curve coefficients here because we only
    // ever use a fixed curve (in our case, the base curve E_0).
    #[allow(non_snake_case)]
    fn double(&self, aPlus2Over4: &PrimeFieldElement) -> ProjectivePrimeFieldPoint {
        let xP = *self;
        // Algorithm 2 of Costello-Smith
        let v1 = (&xP.X + &xP.Z).square();      // (X+Z)^2
        let v2 = (&xP.X - &xP.Z).square();      // (X-Z)^2
        let xz4 = &v1 - &v2;                    // 4XZ = (X+Z)^2 - (X-Z)^2
        let x = &v1 * &v2;                      // (X+Z)^2(X-Z)^2
        let mut v3 = &xz4 * aPlus2Over4;        // 4XZ((a+2)/4)
        v3 = &v3 + &v2;                         // 4XZ((a+2)/4) + (X-Z)^2
        let z = &v3 * &xz4;                     // (4XZ((a+2)/4) + (X-Z)^2)4XZ
        // Now (xQ.x : xQ.z)
        //   = ((X+Z)^2(X-Z)^2 : (4XZ((a + 2)/4) + (X-Z)^2)4XZ )
        ProjectivePrimeFieldPoint{ X: x, Z: z }
    }

    /// Calculates the x-coordinate of `2P` and `P+Q` from the x-coordinate of `P, Q` and `P-Q`.
    // Assumptions:
    // 	  aPlus2Over2 = (A+2)/4.
    //    z(P-Q) = 1,  the Z-coordinate of P-Q is equal to 1.
    // Cost: 6M+4S+8A in Fp
    #[allow(non_snake_case)]
    fn dbl_add(&self, xQ: &ProjectivePrimeFieldPoint, xPmQ: &ProjectivePrimeFieldPoint, aPlus2Over4: &PrimeFieldElement) ->
              (ProjectivePrimeFieldPoint, ProjectivePrimeFieldPoint)
    {
        let xP = *self;
        let x1 = &xPmQ.X;
        let (x2, z2) = (&xP.X, &xP.Z);
        let (x3, z3) = (&xQ.X, &xQ.Z);

        let mut t0 = x2 + z2;   // A = x2+z2
        let mut t1 = x2 - z2;   // B = x2-z2
        let mut t3 = x3 + z3;   // C = x3+z3
        let mut t2 = x3 - z3;   // D = x3-z3
        t2 = &t2 * &t0;         // DA = D*A
        t3 = &t3 * &t1;         // CB = C*B

        let mut x = &t2 + &t3;  // x5 = DA+CB
        let mut z = &t2 - &t3;  // z5 = DA-CB
        x = x.square();         // x5 = (DA+CB)^2
        z = z.square();         // z5 = (DA-CB)^2
        z = &z * x1;            // z5 = x1*(DA-CB)^2
        let xPaddQ = ProjectivePrimeFieldPoint{ X: x, Z: z };

        t0 = t0.square();          // t0 = AA = A^2
        t1 = t1.square();          // t1 = BB = B^2
        x = &t0 * &t1;             // x4 = AA*BB
        t0 = &t0 - &t1;            // t2 = E = AA-BB
        z = &t0 * aPlus2Over4;     // z4 = ((A+2C)/4)*E
        z = &z + &t1;              // z4 = BB+((A+2C)/4)*E
        z = &z * &t0;              // z4 = E*(BB+((A+2C)/4)*E)
        let x2P = ProjectivePrimeFieldPoint{ X: x, Z: z };

        (x2P, xPaddQ)
    }

    /// Given `x(P)` and a scalar m in little-endian bytes, compute `x([m]P), x([m+1]P)`
    /// using the Montgomery ladder. This is described in Algorithm 8 of Costello-Smith.
    ///
    /// The extra value `x([m+1]P)` is returned to allow y-coordinate recovery, otherwise, 
    /// it can be ignored.
    ///
    /// This function's execution time is dependent only on the byte-length of the input
    /// scalar. All scalars of the same input length execute in uniform time.
    /// The scalar can be padded with zero bytes to ensure a uniform length.
    #[allow(non_snake_case)]
    fn scalar_mul_prime_field(&self, aPlus2Over4: &PrimeFieldElement, scalar: &[u8]) -> (ProjectivePrimeFieldPoint, ProjectivePrimeFieldPoint)
    {
        let xP = *self;
        let mut x0 = ProjectivePrimeFieldPoint{ X: PrimeFieldElement::one(), Z: PrimeFieldElement::zero() };
        let mut x1 = xP;

        // Iterate over the bits of the scalar, top to bottom.
        let mut prev_bit: u8 = 0;
        for i in (0..scalar.len()).rev() {
            let scalar_byte = scalar[i];
            for j in (0..8).rev() {
                let bit = (scalar_byte >> (j as u32)) & 0x1;
                ProjectivePrimeFieldPoint::conditional_swap(&mut x0, &mut x1, (bit ^ prev_bit).into());
                assign!{(x0, x1) = x0.dbl_add(&x1, &xP, aPlus2Over4)};
                prev_bit = bit;
            }
        }
        // Now prev_bit is the lowest bit of the scalar.
        ProjectivePrimeFieldPoint::conditional_swap(&mut x0, &mut x1, prev_bit.into());
        (x0, x1)
    }
}

// Sage script for generating test vectors:
// sage: p = 2^372 * 3^239 - 1; Fp = GF(p)
// sage: R.<x> = Fp[]
// sage: Fp2 = Fp.extension(x^2 + 1, 'i')
// sage: i = Fp2.gen()
// sage: A = 4385300808024233870220415655826946795549183378139271271040522089756750951667981765872679172832050962894122367066234419550072004266298327417513857609747116903999863022476533671840646615759860564818837299058134292387429068536219*i + 1408083354499944307008104531475821995920666351413327060806684084512082259107262519686546161682384352696826343970108773343853651664489352092568012759783386151707999371397181344707721407830640876552312524779901115054295865393760
// sage: C = 933177602672972392833143808100058748100491911694554386487433154761658932801917030685312352302083870852688835968069519091048283111836766101703759957146191882367397129269726925521881467635358356591977198680477382414690421049768*i + 9088894745865170214288643088620446862479558967886622582768682946704447519087179261631044546285104919696820250567182021319063155067584445633834024992188567423889559216759336548208016316396859149888322907914724065641454773776307
// sage: E = EllipticCurve(Fp2, [0,A/C,0,1,0])
// sage: X, Y, Z = (8172151271761071554796221948801462094972242987811852753144865524899433583596839357223411088919388342364651632180452081960511516040935428737829624206426287774255114241789158000915683252363913079335550843837650671094705509470594*i + 9326574858039944121604015439381720195556183422719505497448541073272720545047742235526963773359004021838961919129020087515274115525812121436661025030481584576474033630899768377131534320053412545346268645085054880212827284581557, 2381174772709336084066332457520782192315178511983342038392622832616744048226360647551642232950959910067260611740876401494529727990031260499974773548012283808741733925525689114517493995359390158666069816204787133942283380884077*i + 5378956232034228335189697969144556552783858755832284194802470922976054645696324118966333158267442767138528227968841257817537239745277092206433048875637709652271370008564179304718555812947398374153513738054572355903547642836171, 1)
// sage: P = E((X,Y,Z))
// sage: X2, Y2, Z2 = 2*P
// sage: X3, Y3, Z3 = 3*P
// sage: m = 96550223052359874398280314003345143371473380422728857598463622014420884224892
//
#[allow(non_snake_case)]
#[cfg(test)]
mod test {
    use super::*;
    use quickcheck::QuickCheck;

    // A = 4385300808024233870220415655826946795549183378139271271040522089756750951667981765872679172832050962894122367066234419550072004266298327417513857609747116903999863022476533671840646615759860564818837299058134292387429068536219*i + 1408083354499944307008104531475821995920666351413327060806684084512082259107262519686546161682384352696826343970108773343853651664489352092568012759783386151707999371397181344707721407830640876552312524779901115054295865393760
    const CURVE_A: ExtensionFieldElement = ExtensionFieldElement{ A: Fp751Element([0xca2c435e, 0x8319eb18, 0x72cd0267, 0x3a93beae, 0x72fd5a84, 0x5e465e1f, 0x50aa7272, 0x8617fa41, 0x99d62a13, 0x887da247, 0x3c7667fe, 0xb079b31b, 0x0fa14f2e, 0xc4661b15, 0x7bc6efd6, 0xd4d2b296, 0xb7239003, 0x854215a8, 0xcba656c2, 0x61c5302c, 0x7d6f97a2, 0xf93194a2, 0x532bca75, 0x1ed9]),
                                                                  B: Fp751Element([0x0e8c7db6, 0xb6f54104, 0x65342e15, 0x99403e73, 0x7c29cced, 0x457e9cee, 0x073b1d67, 0x8ece72dc, 0x7ad28d28, 0x6e73cef1, 0xca317472, 0x7aed836, 0x54263b54, 0x89e1de94, 0x7aa0071b, 0x74532927, 0x3bc86b9b, 0xf623dfc7, 0xa9245882, 0xb8e3c1d8, 0x17770bec, 0x6ad0b3d3, 0x6e8d502b, 0x5b40]) };
    // C = 933177602672972392833143808100058748100491911694554386487433154761658932801917030685312352302083870852688835968069519091048283111836766101703759957146191882367397129269726925521881467635358356591977198680477382414690421049768*i + 9088894745865170214288643088620446862479558967886622582768682946704447519087179261631044546285104919696820250567182021319063155067584445633834024992188567423889559216759336548208016316396859149888322907914724065641454773776307
    const CURVE_C: ExtensionFieldElement = ExtensionFieldElement{ A: Fp751Element([0xbf723107, 0x4fb2358b, 0xac79e240, 0x3a791521, 0x7c4c922f, 0x283e24ef, 0x205e33cc, 0xc89baa1, 0xcff6fee1, 0x3031be81, 0x2f6a95c4, 0xaf7a494a, 0xaac83a1d, 0x248d251e, 0xe2550c88, 0xc122fca1, 0x1b6cfd3d, 0xbc0451b1, 0xb046222c, 0x9c0a114a, 0x2f21f6ea, 0x43b957b3, 0x87fa61de, 0x5b9c]),
                                                                  B: Fp751Element([0xaac15ec6, 0xacf142af, 0x04a071d5, 0xfd1322a5, 0x10f6c5c6, 0x56bb205e, 0x9a97b9bd, 0xe204d284, 0x02fe7f2e, 0x40b01222, 0xafacf2cb, 0xecf72c6f, 0xf869f60a, 0x45dfc681, 0xff4af66c, 0x11814c9a, 0xeea54fe7, 0x9278b0c4, 0xaf7f2e2e, 0x9a633d5b, 0xf1a05112, 0x69a329e6, 0x4ace23e4, 0x1d87]) };
    
    const CURVE: ProjectiveCurveParameters = ProjectiveCurveParameters{ A: CURVE_A, C: CURVE_C };
    const AFFINE_XP: ExtensionFieldElement = ExtensionFieldElement{ A: Fp751Element([0xaac47247, 0xe8d05f30, 0x55441de7, 0x576ec00c, 0xfe558518, 0xbf1a8ec5, 0x77515881, 0xd77cb17f, 0x7ee73ec4, 0x8e985283, 0xd4f44a6b, 0x8159634a, 0x33a798c5, 0x2e4eb55, 0x4d5bc849, 0x9be8c435, 0x06496b84, 0xf47dc618, 0x295120e0, 0x25d0e130, 0x5f8139e3, 0xdbef5409, 0x4f20862c, 0x5a72]),
                                                                    B: Fp751Element([0x23602e30, 0x3ca30d76, 0xf45f07b7, 0xfb281edd, 0x901a45bc, 0xd2bf62d5, 0x86306dd2, 0xc67c9baf, 0x93f538ca, 0x4e2bd930, 0xc25b9cbe, 0xcfd92075, 0x095bcbab, 0xceafe9a3, 0x80c85414, 0x7d928ad3, 0x2afdc095, 0x37c5f38b, 0xa7b779f4, 0x75325899, 0x49f20fdd, 0xf1305682, 0x264767d1, 0x178f]) };
    const AFFINE_XP2: ExtensionFieldElement = ExtensionFieldElement{ A: Fp751Element([0x576ce979, 0x2a77afa8, 0x9b0aeba0, 0xab1360e6, 0xbffad660, 0xd79e3e3c, 0xa10f106b, 0x5fd0175a, 0xce9fbdbc, 0x1800ebaf, 0x2bdd6166, 0x228fc914, 0x314e34c3, 0x867cf907, 0x4c13c31c, 0xa58d18c9, 0x8b11499f, 0x699a5bc7, 0x01f7ccf1, 0xa29fc29a, 0x347eebce, 0x6c69c0c5, 0xcee0cc57, 0x38e]),
                                                                     B: Fp751Element([0xf4837da0, 0x43607fd5, 0xe27f8f4a, 0x560bad4c, 0x8495b4dd, 0x2164927f, 0xb831a997, 0x621103fd, 0xea7db2db, 0xad740c4e, 0x205096cd, 0x2cde0442, 0xede8324e, 0x2af51a70, 0x0b9f3466, 0x41a4e68, 0x60b8f476, 0x5481f746, 0x56ff4d18, 0xfcb2f3e6, 0x37171acc, 0x42e3ce08, 0x8c30530c, 0x4423]) };
    const AFFINE_XP3: ExtensionFieldElement = ExtensionFieldElement{ A: Fp751Element([0x3feca947, 0x2096e3f2, 0xa4ad8634, 0xf36f635a, 0x983c5e9a, 0xdae3b1c6, 0x62cb74b4, 0xe08df6c2, 0x37452d3d, 0xd2ca4edc, 0x2f500c79, 0xfb5f3fe4, 0xabc2b21f, 0x73740aa3, 0x9f914cca, 0xd535fd86, 0x823fb67f, 0x4a558466, 0xe3bfc715, 0x3e50a7a0, 0x183a132f, 0xf43c6da9, 0xa1e1b8b9, 0x61ac]),
                                                                     B: Fp751Element([0xea5077bd, 0x1e54ec26, 0xd8769f9a, 0x61380572, 0x84f59818, 0xc6151706, 0x3e84ef6e, 0x6309c3b9, 0x18c3fcd0, 0x33c74b13, 0x835afb14, 0xfe8d7956, 0x423c1ecc, 0x2d5a7b55, 0xdfafea68, 0x869db67e, 0x94f0a628, 0x12926323, 0x25bfd141, 0x10bba482, 0xb408daba, 0x6466c28, 0xcfdb7c43, 0x63ca]) };
    const AFFINE_XAP: ExtensionFieldElement = ExtensionFieldElement{ A: Fp751Element([0xd7f938bb, 0x2112f3c7, 0xa4df08f, 0x704a677f, 0x1fb4ef00, 0x825370e3, 0x7469f902, 0xddbf79b, 0x9ea739fd, 0x27640c89, 0xf244108e, 0xfb7b8b19, 0xdd3baebc, 0x546a6679, 0x98d5265f, 0xe9f0ecf3, 0xe75e461, 0x223d2b35, 0xb6aff016, 0x84b322a0, 0x539f8b39, 0xfabe426f, 0xa0604f50, 0x4507]),
                                                                     B: Fp751Element([0x5618a5fe, 0xac77737e, 0xc436ca52, 0xf91c0e08, 0xc323533c, 0xd124037b, 0x52c58b63, 0xc9a772bf, 0x8ef6af4d, 0x3b30c8f3, 0xe134f36e, 0xb9eed160, 0x93b25017, 0x24e38363, 0x11baf1d9, 0xc828be1b, 0x5df50e93, 0x7b7dab58, 0x618bd8e0, 0x1ca3852c, 0xb359fa00, 0x4efa73bc, 0xa923c2d4, 0x50b6]) };

    // m = 96550223052359874398280314003345143371473380422728857598463622014420884224892
    const M_SCALAR_BYTES: [u8; 32] = [124, 123, 149, 250, 180, 117, 108, 72, 140, 23, 85, 180, 73, 245, 30, 163, 11, 49, 240, 164, 166, 129, 173, 148, 81, 17, 231, 245, 91, 125, 117, 213];

    const EXTENSION_FIELD_ELEMENT_ONE: ExtensionFieldElement = ExtensionFieldElement{
            A: Fp751Element([0x249ad, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x83100000, 0x375c6c66, 0x5527b1e4, 0x3f4f24d0, 0x697797bf, 0xac5c4e2e, 0xc89db7b2, 0xd2076956, 0x4ca4b439, 0x7512c7e9, 0x10f7926c, 0x24bce5e2, 0x2d5b]),
            B: Fp751Element([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    };

    const THREE_POINT_LADDER_INPUTS: [ProjectivePoint; 3] = [
        // x(P)
        ProjectivePoint{
            X: ExtensionFieldElement{ A: Fp751Element([0xaac47247, 0xe8d05f30, 0x55441de7, 0x576ec00c, 0xfe558518, 0xbf1a8ec5, 0x77515881, 0xd77cb17f, 0x7ee73ec4, 0x8e985283, 0xd4f44a6b, 0x8159634a, 0x33a798c5, 0x2e4eb55, 0x4d5bc849, 0x9be8c435, 0x6496b84, 0xf47dc618, 0x295120e0, 0x25d0e130, 0x5f8139e3, 0xdbef5409, 0x4f20862c, 0x5a72]), 
                                      B: Fp751Element([0x23602e30, 0x3ca30d76, 0xf45f07b7, 0xfb281edd, 0x901a45bc, 0xd2bf62d5, 0x86306dd2, 0xc67c9baf, 0x93f538ca, 0x4e2bd930, 0xc25b9cbe, 0xcfd92075, 0x95bcbab, 0xceafe9a3, 0x80c85414, 0x7d928ad3, 0x2afdc095, 0x37c5f38b, 0xa7b779f4, 0x75325899, 0x49f20fdd, 0xf1305682, 0x264767d1, 0x178f]) },
            Z: EXTENSION_FIELD_ELEMENT_ONE,
        },
        // x(Q)
        ProjectivePoint{
            X: ExtensionFieldElement{ A: Fp751Element([0x3ad1e10e, 0x2b71a2a9, 0x92cfb333, 0xf0b9842a, 0x15a27f5c, 0xae173736, 0x428330c4, 0x3039239f, 0xed7dcf98, 0xa0c4b735, 0xddf6af6a, 0x6e359771, 0xc4584651, 0xe986e4ca, 0x622d5518, 0x8233a2b, 0xf06b818b, 0xbfd67bf5, 0xf5b966a6, 0xdffe38d0, 0x272ee00a, 0xa86b36a3, 0x2ea4f68f, 0x193e]), 
                                      B: Fp751Element([0x59d9d998, 0x5a0f3964, 0xb1b7dda, 0x479f4225, 0x2a15bf75, 0x4016b57e, 0x3fa3749, 0xc59f9152, 0x399cf8da, 0xd5f90257, 0xd86dcef4, 0x1fb2dadf, 0x429021dc, 0x600f20e6, 0x80c57581, 0x17e347d3, 0x8fe3e440, 0xc1b0d5fa, 0x30ac20e8, 0xbcf0353, 0x6a4f03e6, 0x50c2eb5f, 0x6b7c4571, 0x8]) },
            Z: EXTENSION_FIELD_ELEMENT_ONE,
        },
        // x(P-Q)
        ProjectivePoint{
            X: ExtensionFieldElement{ A: Fp751Element([0x78f7b5ff, 0x4aafa9f3, 0x3aa8eee0, 0x1172a68, 0xbec2c1de, 0xea518d8c, 0x63674557, 0xe191bcbb, 0x7b259011, 0x97bc1963, 0xf4a2e454, 0xdbeae5c9, 0x72a42f95, 0x78f64d1b, 0x7e181e54, 0xe71cb4ea, 0x48543994, 0xe4169d4c, 0x6a98730f, 0x6198c228, 0xbab1afa5, 0xd21d675b, 0x69fce391, 0x2e72]), 
                                      B: Fp751Element([0xce1d0450, 0x23355783, 0x4ce3d93f, 0x683164cf, 0x25970fd8, 0xae6d1c4d, 0xb80b48cf, 0x7807007f, 0xc2bbb8a2, 0xa005a62e, 0x16004cb, 0x6b5649bd, 0x1330176b, 0xbb1a13fa, 0x87660461, 0xbf38e510, 0x5dd7b930, 0xe577fddc, 0x56947cd3, 0x5f38116f, 0x98c36fde, 0x3124f30b, 0xb6e6db37, 0x4ca9]) },
            Z: EXTENSION_FIELD_ELEMENT_ONE,
        },
    ];

    #[test]
    fn one() {
        let tmp = &EXTENSION_FIELD_ELEMENT_ONE * &AFFINE_XP;
        assert!(tmp.vartime_eq(&AFFINE_XP), "Not equal 1");
    }

    #[test]
    fn jinvariant() {
        let j = CURVE.j_invariant();
        // Computed using Sage:
        // j = 3674553797500778604587777859668542828244523188705960771798425843588160903687122861541242595678107095655647237100722594066610650373491179241544334443939077738732728884873568393760629500307797547379838602108296735640313894560419*i + 3127495302417548295242630557836520229396092255080675419212556702820583041296798857582303163183558315662015469648040494128968509467224910895884358424271180055990446576645240058960358037224785786494172548090318531038910933793845
        let known_j = ExtensionFieldElement{
            A: Fp751Element([0x1fb23993, 0xc7a8921c, 0x1327620b, 0xa20aea32, 0xd9676fa8, 0xf1caa17e, 0xb1a04037, 0x61b780e6, 0xc24acc7a, 0x47784af4, 0x300b9adf, 0x83926e2e, 0x6fae5b66, 0xcd891d5, 0xbeb733bc, 0x49b66985, 0x473d518f, 0xd4bcd2a, 0x91abe224, 0xe2422399, 0xf98672f8, 0xa8af5b20, 0x4d4e4d98, 0x139e]),
            B: Fp751Element([0x1f81f359, 0xb5b52a2, 0x5db6d920, 0x715e3a86, 0x8911978b, 0x9bac2f9d, 0xac4c1e3d, 0xef14acd8, 0xcfb09c8, 0xe81aacd9, 0xde4a09d9, 0xaf898288, 0x8c5c4601, 0xb85a7fb8, 0xdd303387, 0x2c37c3f1, 0xe332367c, 0x7ad3277f, 0x25a8e6f8, 0xd4cbee7f, 0x79eaeffa, 0x36eacbe9, 0x5a13ac33, 0x59eb]),
        };

        assert!(j.vartime_eq(&known_j), "Computed incorrect j-invariant: found\n{:?}\nexpected\n{:?}", j, known_j);
    }

    #[test]
    fn projective_point_vartime_eq() {
        let xP = ProjectivePoint{ X: AFFINE_XP, Z: EXTENSION_FIELD_ELEMENT_ONE };
        let mut xQ = xP;
        // Scale xQ, which results in the same projective point.
        xQ.X = &xQ.X * &CURVE_A;
        xQ.Z = &xQ.Z * &CURVE_A;

        assert!(xQ.vartime_eq(&xP), "Expected the scaled point to be equal to the original");
    }

    #[test]
    fn point_double_versus_sage() {
        let xP = ProjectivePoint{ X: AFFINE_XP, Z: EXTENSION_FIELD_ELEMENT_ONE };
        let xQ = xP.pow2k(&CURVE, 1);
        let affine_xQ = xQ.to_affine();

        assert!(affine_xQ.vartime_eq(&AFFINE_XP2), "\nExpected\n{:?}\nfound\n{:?}", AFFINE_XP2, affine_xQ);
    }

    #[test]
    fn point_triple_versus_sage() {
        let xP = ProjectivePoint{ X: AFFINE_XP, Z: EXTENSION_FIELD_ELEMENT_ONE };
        let xQ = xP.pow3k(&CURVE, 1);
        let affine_xQ = xQ.to_affine();

        assert!(affine_xQ.vartime_eq(&AFFINE_XP3), "\nExpected\n{:?}\nfound\n{:?}", AFFINE_XP3, affine_xQ);
    }

    #[test]
    fn point_pow2k_versus_scalar_mul() {
        let byte = [32u8; 1];
        let xP = ProjectivePoint{ X: AFFINE_XP, Z: EXTENSION_FIELD_ELEMENT_ONE };
        let xQ = xP.pow2k(&CURVE, 5);              // = x([32]P)
        let affine_xQ = xQ.to_affine();
        let xR = xP.scalar_mul(&CURVE, &byte[..]); // = x([32]P)
        let affine_xR = xR.to_affine();

        assert!(affine_xQ.vartime_eq(&affine_xR), "\nExpected\n{:?}\nfound\n{:?}", affine_xQ, affine_xR);
    }

    #[test]
    fn scalar_mul_versus_sage() {
        let mut xP = ProjectivePoint{ X: AFFINE_XP, Z: ExtensionFieldElement::one() };
        xP = xP.scalar_mul(&CURVE, &M_SCALAR_BYTES[..]); // = x([m]P)
        let affine_xQ = xP.to_affine();

        assert!(AFFINE_XAP.vartime_eq(&affine_xQ), "\nExpected\n{:?}\nfound\n{:?}", AFFINE_XAP, affine_xQ);
    }

    #[test]
    fn recover_curve_params() {
        // Created using old public key generation code that output the a value:
        let a = ExtensionFieldElement{ A: Fp751Element([0xaaf59ea4, 0x9331d9c5, 0xe4046931, 0xb32b702b, 0x12ed4d34, 0xcebb3339, 0xcd29c7a2, 0x5628ce37, 0x48b7f58e, 0xbeac5ed, 0x81d65b07, 0x1fb9d3e2, 0x1e195662, 0x9c0cfacc, 0x6b70f7d9, 0xae4bce0f, 0x43fe71a0, 0x59e4e63d, 0x60cc8615, 0xef7ce575, 0x901e74e8, 0xe44a8fb7, 0x3c8366d1, 0x69d1]),
                                       B: Fp751Element([0x279ab966, 0xf6da1070, 0x7268c762, 0xa78fb0ce, 0x4a57abfa, 0x19b40f04, 0x60c0c233, 0x7ac8ee61, 0x42947072, 0x93d49934, 0xa4e44860, 0x757d2b3f, 0x8c4d5257, 0x73a920f, 0x54734037, 0x2031f1b0, 0x406555cd, 0xdefaa1d2, 0x1496be3d, 0x26f9c70e, 0xa4d0976, 0x5b3f335a, 0x8b2e9c59, 0x1362]) };
        let affine_xP = ExtensionFieldElement{ A: Fp751Element([0x2aebb250, 0xea6b2d1e, 0xdc4f6386, 0x35d0b205, 0xb1830b8d, 0xb198e93c, 0x496ddcc6, 0x3b5b456b, 0x1132c260, 0x5be3f0d4, 0x7516a00, 0xce5f1888, 0x9ea8866d, 0x54f3e746, 0x47f36286, 0x33809ef, 0xeabe1edb, 0x6fa45f83, 0x5d19fd86, 0x1b3391ae, 0x8584af3f, 0x1e66daf4, 0xc14aaa87, 0xb430]),
                                               B: Fp751Element([0x61dcb2ad, 0x97b41ebc, 0xb932f641, 0x80ead31c, 0x9948b642, 0x40a94009, 0xcdc7fe84, 0x2a22fd16, 0x7579667f, 0xaabf35b1, 0x9feb4032, 0x76c1d013, 0x7b1949be, 0x71467e1e, 0xdd0d6d81, 0x678ca8da, 0xa9064c66, 0x14445dae, 0xb4fa4691, 0x92d161ea, 0x6b238d36, 0x8dfbb01b, 0x18434e4e, 0x2e37]) };
        let affine_xQ = ExtensionFieldElement{ A: Fp751Element([0xa1943439, 0xb055cf0c, 0xfa6c69ed, 0xa9ff5de2, 0x34e5730a, 0x4f2761f9, 0x1f94aa4b, 0x61a1dcaa, 0xfd058543, 0xce3c8fad, 0xa6701b8e, 0xeac432aa, 0x93aea8b, 0x8491d523, 0xd92b9b7f, 0xba273f9b, 0x4439bb5a, 0xd8f59fd3, 0x1c1fe600, 0xdc035026, 0xeb151311, 0x99375ab1, 0x75bbdbc5, 0x14d1]),
                                               B: Fp751Element([0x2111a107, 0xffb0ef8c, 0x25991829, 0x55ceca38, 0xc075d34b, 0xdbf8a1cc, 0xd85d8494, 0xb8e9187b, 0xc34a03b0, 0x670aa2d5, 0x2b064953, 0xef9fe2ed, 0x1d645aee, 0xc911f531, 0x9e410507, 0xf4411f40, 0x2d03e1a8, 0x934a0a85, 0xae1ad544, 0xe6274e67, 0xc69a87bc, 0x9f4bc563, 0x6019681e, 0x6f31]) };
        let affine_xQmP = ExtensionFieldElement{ A: Fp751Element([0x6a153779, 0x6ffb4430, 0xf2f918f3, 0xc0ffef21, 0x5d77f778, 0x196c46d3, 0x52edcfe6, 0x4a73f804, 0xce61c67f, 0x9b00836b, 0x8d84219e, 0x38787941, 0xfc1ec5d1, 0x20700cf9, 0xec64155e, 0x1dfe2356, 0x38256b1c, 0xf8b9e330, 0x4bada0f0, 0xd2aaf2e1, 0x79a4e313, 0xb33b226e, 0x76fad4e5, 0x6be5]),
                                                 B: Fp751Element([0x8e00de34, 0x7db5dbc8, 0xf8b6e11e, 0x75cc8cb9, 0x4ebc52ac, 0x8c8001c0, 0x1a0b5a94, 0x67ef6c98, 0x73230738, 0xc3654fbe, 0x2983ceca, 0xc6a46ee8, 0x27ef49f0, 0xed1aa61a, 0xb0858fe0, 0x17fe5a13, 0x5a4c6b3c, 0x9ae0ca94, 0x18ad8878, 0x234104a2, 0x66104394, 0xa6196271, 0x1ff2e7e, 0x556a]) };

        let curve_params = ProjectiveCurveParameters::recover_curve_parameters(&affine_xP, &affine_xQ, &affine_xQmP);
        let tmp = &curve_params.C.inv() * &curve_params.A;

        assert!(tmp.vartime_eq(&a), "\nExpected\n{:?}\nfound\n{:?}", a, tmp);
    }

    #[test]
    fn three_point_ladder_versus_sage() {
        let xR = ProjectivePoint::three_point_ladder(&THREE_POINT_LADDER_INPUTS[0], &THREE_POINT_LADDER_INPUTS[1], &THREE_POINT_LADDER_INPUTS[2], &CURVE, &M_SCALAR_BYTES[..]);
        let affine_xR = xR.to_affine();
        let sage_affine_xR = ExtensionFieldElement{ A: Fp751Element([0x800d4fd5, 0x729465ba, 0x59e514a1, 0x9398015b, 0xe76c748e, 0x1a59dd6b, 0xb28dd55c, 0x1a7db94e, 0x80b1b8ec, 0x444686e6, 0x2a2454ff, 0xcc3d4ace, 0xec95a419, 0x51d3dab4, 0x94acac6a, 0xc3b0f335, 0x7fd44f8a, 0x9598a74e, 0x8f1c2e37, 0x4fbf8c63, 0x33052f51, 0x844e3470, 0xde3eafcf, 0x6cd6]),
                                                    B: Fp751Element([0x12d73430, 0x85da1454, 0x66eb3232, 0xd83c0e3b, 0x53ec1369, 0xd08ff2d4, 0xdb395b13, 0xa64aaacf, 0xa20e806e, 0xe9cba211, 0x5d937cfc, 0xa4f80b17, 0x4b1f7937, 0x556ce5c6, 0x2b3fdf7a, 0xb59b39ea, 0x9a4196b3, 0xc2526b86, 0xa9371750, 0x8dad90bc, 0x9d9147a2, 0xdfb4a30c, 0x2130629b, 0x346d]) };
        
        assert!(affine_xR.vartime_eq(&sage_affine_xR), "\nExpected\n{:?}\nfound\n{:?}", sage_affine_xR, affine_xR);
    }

    #[test]
    fn right_to_left_ladder_versus_sage() {
        let xR = ProjectivePoint::right_to_left_ladder(&THREE_POINT_LADDER_INPUTS[0], &THREE_POINT_LADDER_INPUTS[1], &THREE_POINT_LADDER_INPUTS[2], &CURVE, &M_SCALAR_BYTES[..]);
        let affine_xR = xR.to_affine();
        let sage_affine_xR = ExtensionFieldElement{ A: Fp751Element([0x800d4fd5, 0x729465ba, 0x59e514a1, 0x9398015b, 0xe76c748e, 0x1a59dd6b, 0xb28dd55c, 0x1a7db94e, 0x80b1b8ec, 0x444686e6, 0x2a2454ff, 0xcc3d4ace, 0xec95a419, 0x51d3dab4, 0x94acac6a, 0xc3b0f335, 0x7fd44f8a, 0x9598a74e, 0x8f1c2e37, 0x4fbf8c63, 0x33052f51, 0x844e3470, 0xde3eafcf, 0x6cd6]),
                                                    B: Fp751Element([0x12d73430, 0x85da1454, 0x66eb3232, 0xd83c0e3b, 0x53ec1369, 0xd08ff2d4, 0xdb395b13, 0xa64aaacf, 0xa20e806e, 0xe9cba211, 0x5d937cfc, 0xa4f80b17, 0x4b1f7937, 0x556ce5c6, 0x2b3fdf7a, 0xb59b39ea, 0x9a4196b3, 0xc2526b86, 0xa9371750, 0x8dad90bc, 0x9d9147a2, 0xdfb4a30c, 0x2130629b, 0x346d]) };
        
        assert!(affine_xR.vartime_eq(&sage_affine_xR), "\nExpected\n{:?}\nfound\n{:?}", sage_affine_xR, affine_xR);
    }

    #[test]
    fn point_triple_versus_add_double() {
        fn triple_equals_add_double(curve: ProjectiveCurveParameters, P: ProjectivePoint) -> bool {
            let cached_params = curve.cached_params();
            let cached_triple_params = curve.cached_triple_params();
            let P2 = P.double(&cached_params);        // = x([2]P)
            let P3 = P.triple(&cached_triple_params); // = x([3]P)
            let P2plusP = P2.add(&P, &P);             // = x([2]P + P)

            P3.vartime_eq(&P2plusP)
        }
        QuickCheck::new().quickcheck(triple_equals_add_double as fn(ProjectiveCurveParameters, ProjectivePoint) -> bool);
    }

    #[test]
    fn scalar_mul_prime_field_and_coordinate_recovery_versus_sage_generated_torsion_points() {
        // x((11,...)) = 11
        let x11 = ProjectivePrimeFieldPoint{
            X: PrimeFieldElement{ A: Fp751Element([0x192a73, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xe6f00000, 0x3916c5c3, 0x19024ab9, 0x68876318, 0x1dcd18cf, 0xc47ba23, 0x7d8c830e, 0x9388299a, 0x3588ea6a, 0xa8e3256c, 0x8259082a, 0x3f160446, 0x3353]) },
            Z: PrimeFieldElement::one(),
        };
        // y((11,...)) = oddsqrt(11^3 + 11)
        let y11 = PrimeFieldElement{ A: Fp751Element([0xf57f3c8a, 0xd38a264d, 0x5042dcdf, 0x9c0450d2, 0x7bbed0b6, 0xaf1ab7be, 0x42b29630, 0xa307981c, 0xe0fa2ecb, 0x845a7e79, 0x32108f55, 0x7ef77ef7, 0x51081f0d, 0x97b58367, 0xf5275ff4, 0x59e3d115, 0x82284916, 0x9a027362, 0x96540e99, 0xec39f711, 0x8dcc965a, 0xf8b521b2, 0xb9d7f54c, 0x6af0]) };
     
        // x((6,...)) = 6
        let x6 =  ProjectivePrimeFieldPoint{
            X: PrimeFieldElement{ A: Fp751Element([0xdba10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x35000000, 0xb8399915, 0x3714fe4e, 0x53eb43f4, 0xc3a25847, 0x5c520428, 0xa3151d60, 0x32c7c978, 0xc116cf52, 0x8efaf6aa, 0x49a84d4b, 0x31e97514, 0x3057]) },
            Z: PrimeFieldElement::one(),
        };
        // y((6,...)) = oddsqrt(6^3 + 6)
        let y6 = PrimeFieldElement{ A: Fp751Element([0xba55ff3c, 0xe4786c67, 0xc2a148e0, 0x6ffa02bc, 0xdf326e2a, 0xe1c5d019, 0xf712e87, 0x23214891, 0xee99c196, 0x6ade324b, 0x6bb821f3, 0x4372f82c, 0x5d391ec4, 0x91a374a1, 0x110b7c75, 0x6e98998b, 0xd4eeb574, 0x2e093f44, 0x68840958, 0x33cdd146, 0x9e353067, 0xb017cea8, 0x7085d4b7, 0x6f90]) };
        // Little-endian bytes of 3^239
        let three_239_bytes: [u8; 48] = [235, 142, 138, 135, 159, 84, 104, 201, 62, 110, 199, 124, 63, 161, 177, 89, 169, 109, 135, 190, 110, 125, 134, 233, 132, 128, 116, 37, 203, 69, 80, 43, 86, 104, 198, 173, 123, 249, 9, 41, 225, 192, 113, 31, 84, 93, 254, 6];
        // Little-endian bytes of 2^372
        let two_372_bytes: [u8; 47] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16];

        // E_0 : y^2 = x^3 + x has a = 0, so (a+2)/4 = 1/2
        let aPlus2Over4 = PrimeFieldElement{ A: Fp751Element([0x124d6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xb8e00000, 0xc0aa7287, 0x9c8a2434, 0xa9a378a3, 0xa206996c, 0x41a41b52, 0x6876280d, 0x175ce04f, 0xe903b49f, 0x666d227, 0xf851186, 0x7cff6e7f, 0x4ea0]) };
        // Compute x(P_A) = x([3^239](11,...)) and x([3^239 + 1](11,...))
        let (xPA, xPAplus11) = x11.scalar_mul_prime_field(&aPlus2Over4, &three_239_bytes[..]);
        // Compute x(P_B) = x([2^372](6,...)) and x([2^372 + 1](6,...))
        let (xPB, xPBplus6) = x6.scalar_mul_prime_field(&aPlus2Over4, &two_372_bytes[..]);

        // Check that the computed x-coordinates are correct:
        let test_affine_xPA = xPA.to_affine();
        assert!(test_affine_xPA.vartime_eq(&AFFINE_X_PA), "Recomputed x(P_A) incorrectly: found\n{:?}\nexpected{:?}\n", AFFINE_X_PA, test_affine_xPA);

        let test_affine_xPB = xPB.to_affine();
        assert!(test_affine_xPB.vartime_eq(&AFFINE_X_PB), "Recomputed x(P_B) incorrectly: found\n{:?}\nexpected{:?}\n", AFFINE_X_PB, test_affine_xPB);

        // Recover y-coordinates and check that those are correct:
        let (mut X_A, mut Y_A, Z_A) = ProjectivePoint::okeya_sakurai_coordinate_recovery(&x11.X, &y11, &xPA, &xPAplus11);
        let invZ_A = Z_A.inv();
        Y_A = &Y_A * &invZ_A; // = Y_A / Z_A
        X_A = &X_A * &invZ_A; // = X_A / Z_A
        assert!(AFFINE_Y_PA.vartime_eq(&Y_A), "Recovered y(P_A) incorrectly: found\n{:?}\nexpected{:?}\n", Y_A, AFFINE_Y_PA);
        assert!(AFFINE_X_PA.vartime_eq(&X_A), "Recovered x(P_A) incorrectly: found\n{:?}\nexpected{:?}\n", X_A, AFFINE_X_PA);

        let (mut X_B, mut Y_B, Z_B) = ProjectivePoint::okeya_sakurai_coordinate_recovery(&x6.X, &y6, &xPB, &xPBplus6);
        let invZ_B = Z_B.inv();
        Y_B = &Y_B * &invZ_B; // = Y_B / Z_B
        X_B = &X_B * &invZ_B; // = X_B / Z_B
        assert!(AFFINE_Y_PB.vartime_eq(&Y_B), "Recovered y(P_B) incorrectly: found\n{:?}\nexpected{:?}\n", Y_B, AFFINE_Y_PB);
        assert!(AFFINE_X_PB.vartime_eq(&X_B), "Recovered x(P_B) incorrectly: found\n{:?}\nexpected{:?}\n", X_B, AFFINE_X_PB);
   }
}
