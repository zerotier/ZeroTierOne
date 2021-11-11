// This file is part of sidh-rs.
// Copyright (c) 2017 Erkan Tairi
// See LICENSE for licensing information.
//
// Author:
// - Erkan Tairi <erkan.tairi@gmail.com>
//

//! This module contains internal isogeny representation and operations 
//! for SIDH, which is not part of the public API.

use crate::sidhp751::field::ExtensionFieldElement;
use crate::sidhp751::curve::{ProjectiveCurveParameters, ProjectivePoint};

/// Represents a 3-isogeny phi, holding the data necessary to evaluate phi.
#[allow(non_snake_case)]
#[derive(Copy, Clone)]
pub struct ThreeIsogeny {
    pub X: ExtensionFieldElement,
    pub Z: ExtensionFieldElement,
}

#[allow(non_snake_case)]
impl ThreeIsogeny {
    /// Given a three-torsion point `x3 = x(P_3)` on the curve `E_(A:C)`, construct the
    /// three-isogeny `phi : E_(A:C) -> E_(A:C)/<P_3> = E_(A':C')`.
    ///
    /// Returns a tuple `(codomain, isogeny) = (E_(A':C'), phi)`.
    pub fn compute_three_isogeny(x3: &ProjectivePoint) -> (ProjectiveCurveParameters, ThreeIsogeny) {
        let isogeny = ThreeIsogeny{ X: x3.X, Z: x3.Z };
        // We want to compute
	    // (A':C') = (Z^4 + 18X^2Z^2 - 27X^4 : 4XZ^3)
	    // To do this, use the identity 18X^2Z^2 - 27X^4 = 9X^2(2Z^2 - 3X^2)
        let mut v1 = x3.X.square();      // = X^2
        let mut v0 = &(&v1 + &v1) + &v1; // = 3X^2
        v1 = &(&v0 + &v0) + &v0;         // = 9X^2
        let mut v2 = x3.Z.square();      // = Z^2
        let v3 = v2.square();            // = Z^4
        v2 = &v2 + &v2;                  // = 2Z^2
        v0 = &v2 - &v0;                  // = 2Z^2 - 3X^2
        v1 = &v1 * &v0;                  // = 9X^2(2Z^2 - 3X^2)
        v0 = &x3.X * &x3.Z;              // = XZ
        v0 = &v0 + &v0;                  // = 2XZ
        let a = &v3 + &v1;               // = Z^4 + 9X^2(2Z^2 - 3X^2)
        let c = &v0 * &v2;               // = 4XZ^3
        let codomain = ProjectiveCurveParameters{ A: a, C: c };

        (codomain, isogeny)
    }

    /// Given a 3-isogeny phi and a point `xP = x(P)`, compute `x(Q)`, the x-coordinate
    /// of the image `Q = phi(P)` of `P` under `phi : E_(A:C) -> E_(A':C')`.
    ///
    /// The output `xQ = x(Q)` is then a point on the curve `E_(A':C')`; the curve
    /// parameters are returned by the compute_three_isogeny function used to construct
    /// phi.
    pub fn eval(&self, xP: &ProjectivePoint) -> ProjectivePoint {
        let phi = *self;
        let mut t0 = &phi.X * &xP.X; // = X3*XP
        let mut t1 = &phi.Z * &xP.Z; // = Z3*XP
        let mut t2 = &t0 - &t1;      // = X3*XP - Z3*ZP
        t0 = &phi.Z * &xP.X;         // = Z3*XP
        t1 = &phi.X * &xP.Z;         // = X3*ZP
        t0 = &t0 - &t1;              // = Z3*XP - X3*ZP
        t2 = t2.square();            // = (X3*XP - Z3*ZP)^2
        t0 = t0.square();            // = (Z3*XP - X3*ZP)^2
        let x = &t2 * &xP.X;         // = XP*(X3*XP - Z3*ZP)^2
        let z = &t0 * &xP.Z;         // = ZP*(Z3*XP - X3*ZP)^2
        let xQ = ProjectivePoint{ X: x, Z: z };

        xQ
    }
}

/// Represents a 4-isogeny phi, holding the data necessary to evaluate phi.
//
// See compute_four_isogeny for more details.
#[allow(non_snake_case)]
#[derive(Copy, Clone)]
pub struct FourIsogeny {
    pub Xsq_plus_Zsq : ExtensionFieldElement,
    pub Xsq_minus_Zsq: ExtensionFieldElement,
    pub XZ2          : ExtensionFieldElement,
    pub Xpow4        : ExtensionFieldElement,
    pub Zpow4        : ExtensionFieldElement,
}

#[allow(non_snake_case)]
impl FourIsogeny {
    /// Given a four-torsion point `x4 = x(P_4)` on the curve `E_(A:C)`, compute the
    /// coefficients of the codomain `E_(A':C')` of the four-isogeny `phi : E_(A:C) ->
    /// E_(A:C)/<P_4>`.
    ///
    /// Returns a tuple `(codomain, isogeny) = (E_(A':C') : phi)`.
    //
    // There are two sets of formulas in Costello-Longa-Naehrig for computing
    // four-isogenies. One set is for the case where (1,...) lies in the kernel of
    // the isogeny (this is the FirstFourIsogeny), and the other (this set) is for
    // the case that (1,...) is *not* in the kernel.
    pub fn compute_four_isogeny(x4: &ProjectivePoint) -> (ProjectiveCurveParameters, FourIsogeny) {
        let mut v0 = x4.X.square();    // = X4^2
        let v1 = x4.Z.square();        // = Z4^2
        let Xsq_plus_Zsq = &v0 + &v1;  // = X4^2 + Z4^2
        let Xsq_minus_Zsq = &v0 - &v1; // = X4^2 - Z4^2
        let mut XZ2 = &x4.X + &x4.Z;   // = X4 + Z4
        XZ2 = XZ2.square();            // = X4^2 + Z4^2 + 2X4Z4
        XZ2 = &XZ2 - &Xsq_plus_Zsq;    // = 2X4Z4
        let Xpow4 = v0.square();       // = X4^4
        let Zpow4 = v1.square();       // = Z4^4
        v0 = &Xpow4 + &Xpow4;          // = 2X4^4
        v0 = &v0 - &Zpow4;             // = 2X4^4 - Z4^4
        let a = &v0 + &v0;             // = 2(2X4^4 - Z4^4)
        let c = Zpow4;                 // = Z4^4

        let codomain = ProjectiveCurveParameters{ A: a, C: c };
        let isogeny = FourIsogeny{
            Xsq_plus_Zsq,
            Xsq_minus_Zsq,
            XZ2,
            Xpow4,
            Zpow4
        };

        (codomain, isogeny)
    }

    /// Given a 4-isogeny phi and a point `xP = x(P)`, compute `x(Q)`, the x-coordinate
    /// of the image `Q = phi(P)` of `P` under `phi : E_(A:C) -> E_(A':C')`.
    //
    // The output xQ = x(Q) is then a point on the curve E_(A':C'); the curve
    // parameters are returned by the compute_four_isogeny function used to construct
    // phi.
    pub fn eval(&self, xP: &ProjectivePoint) -> ProjectivePoint {
        let phi = *self;
        // We want to compute formula (7) of Costello-Longa-Naehrig, namely
        //
        // Xprime = (2*X_4*Z*Z_4 - (X_4^2 + Z_4^2)*X)*(X*X_4 - Z*Z_4)^2*X
        // Zprime = (2*X*X_4*Z_4 - (X_4^2 + Z_4^2)*Z)*(X_4*Z - X*Z_4)^2*Z
        //
        // To do this we adapt the method in the MSR implementation, which computes
        //
        // X_Q = Xprime*( 16*(X_4 + Z_4)*(X_4 - Z_4)*X_4^2*Z_4^4 )
        // Z_Q = Zprime*( 16*(X_4 + Z_4)*(X_4 - Z_4)*X_4^2*Z_4^4 )
        //
        let mut t0 = &xP.X * &phi.XZ2;          // = 2*X*X_4*Z_4
        let mut t1 = &xP.Z * &phi.Xsq_plus_Zsq; // = (X_4^2 + Z_4^2)*Z
        t0 = &t0 - &t1;                         // = -X_4^2*Z + 2*X*X_4*Z_4 - Z*Z_4^2
        t1 = &xP.Z * &phi.Xsq_minus_Zsq;        // = (X_4^2 - Z_4^2)*Z
        let mut t2 = (&t0 - &t1).square();      // = 4*(X_4*Z - X*Z_4)^2*X_4^2
        t0 = &t0 * &t1;
        t0 = &t0 + &t0;
        t0 = &t0 + &t0;                         // = 4*(2*X*X_4*Z_4 - (X_4^2 + Z_4^2)*Z)*(X_4^2 - Z_4^2)*Z
        t1 = &t0 + &t2;                         // = 4*(X*X_4 - Z*Z_4)^2*Z_4^2
        t0 = &t0 * &t2;                         // = Zprime * 16*(X_4 + Z_4)*(X_4 - Z_4)*X_4^2
        let z = &t0 * &phi.Zpow4;               // = Zprime * 16*(X_4 + Z_4)*(X_4 - Z_4)*X_4^2*Z_4^4
        t2 = &t2 * &phi.Zpow4;                  // = 4*(X_4*Z - X*Z_4)^2*X_4^2*Z_4^4
        t0 = &t1 * &phi.Xpow4;                  // = 4*(X*X_4 - Z*Z_4)^2*X_4^4*Z_4^2
        t0 = &t2 - &t0;                         // = -4*(X*X_4^2 - 2*X_4*Z*Z_4 + X*Z_4^2)*X*(X_4^2 - Z_4^2)*X_4^2*Z_4^2
        let x = &t1 * &t0;                      // = Xprime * 16*(X_4 + Z_4)*(X_4 - Z_4)*X_4^2*Z_4^4
        let xQ = ProjectivePoint{ X: x, Z: z };

        xQ
    }
}

/// Represents a 4-isogeny phi. 
//
// See compute_four_isogeny for details.
#[allow(non_snake_case)]
#[derive(Copy, Clone)]
pub struct FirstFourIsogeny {
    pub A: ExtensionFieldElement,
    pub C: ExtensionFieldElement,
}

#[allow(non_snake_case)]
impl FirstFourIsogeny {
    /// Compute the "first" four-isogeny from the given curve. 
    //
    //  See also compute_four_isogeny and Costello-Longa-Naehrig for more details.
    pub fn compute_first_four_isogeny(domain: &ProjectiveCurveParameters) -> (ProjectiveCurveParameters, FirstFourIsogeny) {
        let mut t0 = &domain.C + &domain.C; // = 2*C
        let c = &domain.A - &t0;            // = A - 2*C
        let mut t1 = &t0 + &t0;             // = 4*C
        t1 = &t1 + &t0;                     // = 6*C
        t0 = &t1 + &domain.A;               // = A + 6*C
        let a = &t0 + &t0;                  // = 2*(A + 6*C)
        
        let codomain = ProjectiveCurveParameters{ A: a, C: c };
        let isogeny = FirstFourIsogeny{ A: domain.A, C: domain.C };

        (codomain, isogeny)
    }

    /// Given a 4-isogeny phi and a point `xP = x(P)`, compute `x(Q)`, the x-coordinate
    /// of the image `Q = phi(P)` of `P` under `phi : E_(A:C) -> E_(A':C')`.
    //
    // The output xQ = x(Q) is then a point on the curve E_(A':C'); the curve
    // parameters are returned by the compute_first_four_isogeny function used to 
    // construct phi.
    pub fn eval(&self, xP: &ProjectivePoint) -> ProjectivePoint {
        let phi = *self;
        let mut t0 = (&xP.X + &xP.Z).square(); // = (X+Z)^2
        let t2 = &xP.X * &xP.Z;                // = X*Z
        let mut t1 = &t2 + &t2;                // = 2*X*Z
        t1 = &t0 - &t1;                        // = X^2 + Z^2
        let mut x = &phi.A * &t2;              // = A*X*Z
        let t3 = &phi.C * &t1;                 // = C*(X^2 + Z^2)
        x = &x + &t3;                          // = A*X*Z + C*(X^2 + Z^2)
        x = &x * &t0;                          // = (X+Z)^2 * (A*X*Z + C*(X^2 + Z^2))
        t0 = (&xP.X - &xP.Z).square();         // = (X-Z)^2
        t0 = &t0 * &t2;                        // = X*Z*(X-Z)^2
        t1 = &phi.C + &phi.C;                  // = 2*C
        t1 = &t1 - &phi.A;                     // = 2*C - A
        let z = &t1 * &t0;                     // = (2*C - A)*X*Z*(X-Z)^2
        let xQ = ProjectivePoint{ X: x, Z: z };

        xQ
    }
}

#[allow(non_snake_case)]
#[cfg(test)]
mod test {
    use super::*;
    use crate::sidhp751::fp::Fp751Element;

    // Test the first four-isogeny from the base curve E_0(F_{p^2}).
    #[test]
    fn first_four_isogeny_versus_sage() {
        // sage: p = 2^372 * 3^239 - 1; Fp = GF(p)
        // sage: R.<x> = Fp[]
        // sage: Fp2 = Fp.extension(x^2 + 1, 'i')
        // sage: i = Fp2.gen()
        // sage: E0Fp = EllipticCurve(Fp, [0,0,0,1,0])
        // sage: E0Fp2 = EllipticCurve(Fp2, [0,0,0,1,0])
        // sage: x_PA = 11
        // sage: y_PA = -Fp(11^3 + 11).sqrt()
        // sage: x_PB = 6
        // sage: y_PB = -Fp(6^3 + 6).sqrt()
        // sage: P_A = 3^239 * E0Fp((x_PA,y_PA))
        // sage: P_B = 2^372 * E0Fp((x_PB,y_PB))
        // sage: def tau(P):
        // ....:     return E0Fp2( (-P.xy()[0], i*P.xy()[1]))
        // ....:
        // sage: m_B = 3*randint(0,3^238)
        // sage: m_A = 2*randint(0,2^371)
        // sage: R_A = E0Fp2(P_A) + m_A*tau(P_A)
        // sage: def y_recover(x, a):
        // ....:     return (x**3 + a*x**2 + x).sqrt()
        // ....:
        // sage: first_4_torsion_point = E0Fp2(1, y_recover(Fp2(1),0))
        // sage: sage_first_4_isogeny = E0Fp2.isogeny(first_4_torsion_point)
        // sage: a = Fp2(0)
        // sage: sage_isomorphism = sage_first_4_isogeny.codomain().isomorphism_to(EllipticCurve(Fp2, [0,(2*(a+6))/(a-2),0,1,0]))
        // sage: isogenized_R_A = sage_isomorphism(sage_first_4_isogeny(R_A))
        //
        let xR = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x2a95fce9, 0xa179cb7e, 0xa0a892c0, 0xbfd6a0f3, 0x250ab3f3, 0x8b2f0aa4, 0x4118732d, 0x2e7aa4dd, 0x93acbc2a, 0x627969e4, 0xc7b8cc83, 0x21a5b852, 0x586324f2, 0x26084278, 0x5aa947c0, 0x383be1aa, 0xb5c0183e, 0xc6558ecb, 0x6a52b035, 0xf1f19208, 0x5b865c1b, 0x4c58b75, 0xceea2d2c, 0x67b4]),
            B: Fp751Element([0x797fecbf, 0xfceb02a2, 0x21f95e99, 0x3fee9e1d, 0x6024e166, 0xa1c4ce89, 0x54517358, 0xc09c0242, 0xb17b94e7, 0xf0255994, 0xb41ee894, 0xa4834359, 0xb7ebefbe, 0x9487f7d, 0xa0bf1f24, 0x3bbeeb34, 0x514c6a05, 0xfa7e5533, 0x46450a9a, 0x92b03281, 0xfada4c06, 0xfde71ca3, 0xf995c2bd, 0x3610]) 
        });
        
        let sage_isogenized_xR = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x78da1e05, 0xff99e76f, 0xbb8d97c4, 0xdaa36bd2, 0xa409daf, 0xb4328cee, 0x80c5da3f, 0xc28b0999, 0xcfebb852, 0xf2d7cd15, 0xded6cdef, 0x1935103d, 0xde1429c3, 0xade81528, 0x90a64319, 0x6775b0fa, 0xee52485d, 0x25f89817, 0x848e697, 0x706e2d0, 0x216d65c0, 0xc4958ec4, 0x9681417f, 0xc51]), 
            B: Fp751Element([0xe60e1fb9, 0x742fe7dd, 0x466a456b, 0x801a3c78, 0x86f48c35, 0xa9f945b7, 0xb144348f, 0x20ce89e1, 0x7776217e, 0xf633970b, 0xb38976e5, 0x4c6077a9, 0x766c7825, 0x34a513fc, 0x59b9cd65, 0xacccba3, 0xf0fd0125, 0xd0ca8383, 0x7196287a, 0x7735043, 0x6d4ea21, 0x9fe1ad77, 0x129ee42d, 0x4d26]) 
        });

        let curve_params = ProjectiveCurveParameters{ A: ExtensionFieldElement::zero(), C: ExtensionFieldElement::one() };

        let (_, phi) = FirstFourIsogeny::compute_first_four_isogeny(&curve_params);
        let isogenized_xR = phi.eval(&xR);

        assert!(sage_isogenized_xR.vartime_eq(&isogenized_xR), 
                "\nExpected\n{:?}\nfound\n{:?}", sage_isogenized_xR.to_affine(), isogenized_xR.to_affine());
    }

    #[test]
    fn four_isogeny_versus_sage() {
        // sage: p = 2^372 * 3^239 - 1; Fp = GF(p)
        //   ***   Warning: increasing stack size to 2000000.
        //   ***   Warning: increasing stack size to 4000000.
        // sage: R.<x> = Fp[]
        // sage: Fp2 = Fp.extension(x^2 + 1, 'i')
        // sage: i = Fp2.gen()
        // sage: E0Fp = EllipticCurve(Fp, [0,0,0,1,0])
        // sage: E0Fp2 = EllipticCurve(Fp2, [0,0,0,1,0])
        // sage: x_PA = 11
        // sage: y_PA = -Fp(11^3 + 11).sqrt()
        // sage: x_PB = 6
        // sage: y_PB = -Fp(6^3 + 6).sqrt()
        // sage: P_A = 3^239 * E0Fp((x_PA,y_PA))
        // sage: P_B = 2^372 * E0Fp((x_PB,y_PB))
        // sage: def tau(P):
        // ....:     return E0Fp2( (-P.xy()[0], i*P.xy()[1]))
        // ....:
        // sage: m_B = 3*randint(0,3^238)
        // sage: m_A = 2*randint(0,2^371)
        // sage: R_A = E0Fp2(P_A) + m_A*tau(P_A)
        // sage: def y_recover(x, a):
        // ....:     return (x**3 + a*x**2 + x).sqrt()
        // ....:
        // sage: first_4_torsion_point = E0Fp2(1, y_recover(Fp2(1),0))
        // sage: sage_first_4_isogeny = E0Fp2.isogeny(first_4_torsion_point)
        // sage: a = Fp2(0)
        // sage: E1A = EllipticCurve(Fp2, [0,(2*(a+6))/(a-2),0,1,0])
        // sage: sage_isomorphism = sage_first_4_isogeny.codomain().isomorphism_to(E1A)
        // sage: isogenized_R_A = sage_isomorphism(sage_first_4_isogeny(R_A))
        // sage: P_4 = (2**(372-4))*isogenized_R_A
        // sage: P_4._order = 4 #otherwise falls back to generic group methods for order
        // sage: X4, Z4 = P_4.xy()[0], 1
        // sage: phi4 = EllipticCurveIsogeny(E1A, P_4, None, 4)
        // sage: E2A_sage = phi4.codomain() # not in monty form
        // sage: Aprime, Cprime = 2*(2*X4^4 - Z4^4), Z4^4
        // sage: E2A = EllipticCurve(Fp2, [0,Aprime/Cprime,0,1,0])
        // sage: sage_iso = E2A_sage.isomorphism_to(E2A)
        // sage: isogenized2_R_A = sage_iso(phi4(isogenized_R_A))
        //
        let xR = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x78da1e05, 0xff99e76f, 0xbb8d97c4, 0xdaa36bd2, 0xa409daf, 0xb4328cee, 0x80c5da3f, 0xc28b0999, 0xcfebb852, 0xf2d7cd15, 0xded6cdef, 0x1935103d, 0xde1429c3, 0xade81528, 0x90a64319, 0x6775b0fa, 0xee52485d, 0x25f89817, 0x848e697, 0x706e2d0, 0x216d65c0, 0xc4958ec4, 0x9681417f, 0xc51]), 
            B: Fp751Element([0xe60e1fb9, 0x742fe7dd, 0x466a456b, 0x801a3c78, 0x86f48c35, 0xa9f945b7, 0xb144348f, 0x20ce89e1, 0x7776217e, 0xf633970b, 0xb38976e5, 0x4c6077a9, 0x766c7825, 0x34a513fc, 0x59b9cd65, 0xacccba3, 0xf0fd0125, 0xd0ca8383, 0x7196287a, 0x7735043, 0x6d4ea21, 0x9fe1ad77, 0x129ee42d, 0x4d26]) 
        });
        
        let xP4 = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x13f3d5e7, 0x2afd75a9, 0x6f88c9ab, 0x2918fba0, 0xcb526f05, 0xa4ac4dc7, 0x1a607300, 0x2d19e939, 0x34091b54, 0x7a79e2b, 0xb42f1792, 0x3ad809dc, 0x8bd6402a, 0xd4617932, 0x1e2c4f3f, 0x1afa7354, 0xce9bdbd8, 0xf602d73a, 0x6bab7004, 0xd77ac58f, 0x6793b3b3, 0x4689d97f, 0xb00e42b7, 0x4f26]), 
            B: Fp751Element([0xafdcb890, 0x6cdf918d, 0xc29cfae2, 0x666f273c, 0x1ba618e2, 0xad00fcd3, 0xef2f6a33, 0x5fbcf62b, 0x318e5098, 0xf408bb88, 0x9453d175, 0x84ab9784, 0xcfb8e1ac, 0x501bbfcd, 0xe6b5542c, 0xf2370098, 0xf0f6bd32, 0xc7dc73f5, 0x6729d1cf, 0xdd76dcd8, 0x29996e4, 0xca22c905, 0xa9373de3, 0x5cf4]) 
        });
        
        let sage_isogenized_xR = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0xd0b7a01e, 0x111efd8b, 0x3789ca9b, 0x6ab75a4f, 0x8564cac4, 0x939dbe51, 0x601d0434, 0xf9eeaba1, 0x6edac998, 0x8d41f8ba, 0xfe9aa170, 0xfcd2557e, 0x98b7844, 0xb3c3549c, 0x6f81127c, 0x52874fef, 0xaa518bb3, 0xb2b9ac82, 0x30520a86, 0xee708202, 0x5efb184a, 0xd4012b7f, 0x4536329b, 0x573e]), 
            B: Fp751Element([0x1e932902, 0xa9995228, 0x71f2c7b1, 0x569a89a5, 0x46ba3f6b, 0x61501438, 0x41e91430, 0x11fd2044, 0x55c9b07b, 0x7f469bd, 0xde35b161, 0xb72db8b9, 0xa940512a, 0x455a9a37, 0xabaf906, 0xb0cff767, 0x583375fe, 0x18c785b7, 0x403c9148, 0x603ab9ca, 0x6e6c62c1, 0xab54ba3a, 0xd7d57c4f, 0x2726]) 
        });

        let (_, phi) = FourIsogeny::compute_four_isogeny(&xP4);
        let isogenized_xR = phi.eval(&xR);

        assert!(sage_isogenized_xR.vartime_eq(&isogenized_xR), 
                "\nExpected\n{:?}\nfound\n{:?}", sage_isogenized_xR.to_affine(), isogenized_xR.to_affine());
    }

    #[test]
    fn three_isogeny_versus_sage() {
        // sage: %colors Linux
        // sage: p = 2^372 * 3^239 - 1; Fp = GF(p)
        //   ***   Warning: increasing stack size to 2000000.
        //   ***   Warning: increasing stack size to 4000000.
        // sage: R.<x> = Fp[]
        // sage: Fp2 = Fp.extension(x^2 + 1, 'i')
        // sage: i = Fp2.gen()
        // sage: E0Fp = EllipticCurve(Fp, [0,0,0,1,0])
        // sage: E0Fp2 = EllipticCurve(Fp2, [0,0,0,1,0])
        // sage: x_PA = 11
        // sage: y_PA = -Fp(11^3 + 11).sqrt()
        // sage: x_PB = 6
        // sage: y_PB = -Fp(6^3 + 6).sqrt()
        // sage: P_A = 3^239 * E0Fp((x_PA,y_PA))
        // sage: P_B = 2^372 * E0Fp((x_PB,y_PB))
        // sage: def tau(P):
        // ....:     return E0Fp2( (-P.xy()[0], i*P.xy()[1]))
        // ....:
        // sage: m_B = 3*randint(0,3^238)
        // sage: R_B = E0Fp2(P_B) + m_B*tau(P_B)
        // sage: P_3 = (3^238)*R_B
        // sage: def three_isog(P_3, P):
        // ....:     X3, Z3 = P_3.xy()[0], 1
        // ....:     XP, ZP = P.xy()[0], 1
        // ....:     x = (XP*(X3*XP - Z3*ZP)^2)/(ZP*(Z3*XP - X3*ZP)^2)
        // ....:     A3, C3 = (Z3^4 + 9*X3^2*(2*Z3^2 - 3*X3^2)), 4*X3*Z3^3
        // ....:     cod = EllipticCurve(Fp2, [0,A3/C3,0,1,0])
        // ....:     return cod.lift_x(x)
        // ....:
        // sage: isogenized_R_B = three_isog(P_3, R_B)
        //
        let xR = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x5cc9a3d7, 0xbd0737ed, 0x6517c101, 0x45ae6d47, 0x7364fdb2, 0x6f228e9e, 0x225b3dbd, 0xbba4871, 0xe5da1a07, 0x6299ccd2, 0xaf5f2d0e, 0x38488fe4, 0xa86e980c, 0xec23cae5, 0x3f1edffa, 0x26c804ba, 0x32df60e5, 0xfbbed819, 0x82ae9187, 0x7e00e9d1, 0x66d05f4b, 0xc7654abb, 0x567237b, 0x262d]), 
            B: Fp751Element([0xd0b2ac33, 0x3a3b5b6a, 0x179127d3, 0x246602b5, 0xad65077d, 0x502ae0e9, 0x37e1bf70, 0x10a3a372, 0x4dd05610, 0x4a1ab929, 0x30fe1fa6, 0xb0f3adac, 0x7faf70cb, 0x34199526, 0x39cf4ec1, 0xa14dd94d, 0xd1bf5568, 0xce4b7527, 0xed45c7e4, 0xe0410423, 0xb6425686, 0x38011809, 0x2472ebed, 0x28f5]) 
        });

        let xP3 = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x7b0788dc, 0x7bb7a4a0, 0x607b21b0, 0xdc36a3f6, 0xe74cf2f0, 0x4750e18e, 0xb7ab806, 0x464e319d, 0x4f758ff, 0xc25aa44c, 0xa46e0a68, 0x392e8521, 0x3eff37df, 0xfc4e76b6, 0x92e67dd8, 0x1f3566d8, 0x73295e65, 0xf8d2eb0f, 0xc470bccb, 0x457b13eb, 0xfef5be33, 0xfda1cc9e, 0x3d92cc02, 0x5dbf]), 
            B: Fp751Element([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]) 
        });

        let sage_isogenized_xR = ProjectivePoint::from_affine(&ExtensionFieldElement{
            A: Fp751Element([0x5913c5b1, 0x286db7d7, 0x50189220, 0xcb2049ad, 0x765fa9f4, 0xccee90ef, 0x730e7d88, 0x65e52ce2, 0xbd0d06e7, 0xa6b6b553, 0x14591590, 0xb561ecec, 0x8c64d959, 0x17b7a66d, 0xcbe1461e, 0x77778ce, 0xc41a57ce, 0x9405c9c0, 0xe8ca7d3d, 0x8f6b4847, 0x7b366937, 0xf625eb98, 0x3590e345, 0x421b]), 
            B: Fp751Element([0x3e7d8d6, 0x566b8938, 0xd527e696, 0xe8c71a04, 0x7bf5eb51, 0x5a1d8f8, 0xe098724f, 0x42ae08a, 0xaf40ca2e, 0x4ee3d7c7, 0x67bb10a7, 0xd9f9ab90, 0xedd6328c, 0xecd53d69, 0x2dea107d, 0xa581e920, 0x8ecf9257, 0x8bcdfb6c, 0x5cbcf2af, 0xe7cbbc2e, 0x1f0e53e, 0x5f031a87, 0x2d93e3cb, 0x1831]) 
        });

        let (_, phi) = ThreeIsogeny::compute_three_isogeny(&xP3);
        let isogenized_xR = phi.eval(&xR);

        assert!(sage_isogenized_xR.vartime_eq(&isogenized_xR), 
                "\nExpected\n{:?}\nfound\n{:?}", sage_isogenized_xR.to_affine(), isogenized_xR.to_affine());
    }
}
