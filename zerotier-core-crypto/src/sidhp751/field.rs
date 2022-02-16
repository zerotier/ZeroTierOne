// This file is part of sidh-rs.
// Copyright (c) 2017 Erkan Tairi
// See LICENSE for licensing information.
//
// Author:
// - Erkan Tairi <erkan.tairi@gmail.com>
//

//! This module contains finite field arithmetic functionality for SIDH, 
//! which is not part of the public API.

use crate::sidhp751::fp::*;

use std::fmt::Debug;

use std::cmp::{Eq, PartialEq};
use std::ops::*;

use subtle::ConditionallySelectable;
use subtle::ConstantTimeEq;
use subtle::Choice;

#[cfg(test)]
use quickcheck::{Arbitrary, Gen};

//-----------------------------------------------------------------------------//
//                           Extension Field                                   //
//-----------------------------------------------------------------------------//

/// Represents an element of the extension field `F_{p^2}`.
#[allow(non_snake_case)]
#[derive(Copy, Clone, PartialEq)]
pub struct ExtensionFieldElement {
    /// This field element is in Montgomery form, so that the value `A` is
    /// represented by `aR mod p`.
    pub A: Fp751Element,
    /// This field element is in Montgomery form, so that the value `B` is
    /// represented by `bR mod p`.
    pub B: Fp751Element,
}

impl<'b> AddAssign<&'b ExtensionFieldElement> for ExtensionFieldElement {
    #[inline(always)]
    fn add_assign(&mut self, _rhs: &'b ExtensionFieldElement) {
        let result = (self as &ExtensionFieldElement) + _rhs;
        self.A = result.A;
        self.B = result.B;
    }
}

impl<'a, 'b> Add<&'b ExtensionFieldElement> for &'a ExtensionFieldElement {
    type Output = ExtensionFieldElement;
    #[inline(always)]
    fn add(self, _rhs: &'b ExtensionFieldElement) -> ExtensionFieldElement {
        let a = &self.A + &_rhs.A;
        let b = &self.B + &_rhs.B;

        ExtensionFieldElement{
            A: a,
            B: b
        }
    }
}

impl <'b> SubAssign<&'b ExtensionFieldElement> for ExtensionFieldElement {
    #[inline(always)]
    fn sub_assign(&mut self, _rhs: &'b ExtensionFieldElement) {
        let result = (self as &ExtensionFieldElement) - _rhs;
        self.A = result.A;
        self.B = result.B;
    }
}

impl<'a, 'b> Sub<&'b ExtensionFieldElement> for &'a ExtensionFieldElement {
    type Output = ExtensionFieldElement;
    #[inline(always)]
    fn sub(self, _rhs: &'b ExtensionFieldElement) -> ExtensionFieldElement {
        let a = &self.A - &_rhs.A;
        let b = &self.B - &_rhs.B;

        ExtensionFieldElement{
            A: a,
            B: b
        }
    }
}

impl<'b> MulAssign<&'b ExtensionFieldElement> for ExtensionFieldElement {
    #[inline(always)]
    fn mul_assign(&mut self, _rhs: &'b ExtensionFieldElement) {
        let result = (self as &ExtensionFieldElement) * _rhs;
        self.A = result.A;
        self.B = result.B;
    }
}

impl<'a, 'b> Mul<&'b ExtensionFieldElement> for &'a ExtensionFieldElement {
    type Output = ExtensionFieldElement;
    fn mul(self, _rhs: &'b ExtensionFieldElement) -> ExtensionFieldElement {
        // Alias self, _rhs for more readable formulas.
        let a = &self.A;
        let b = &self.B;
        let c = &_rhs.A;
        let d = &_rhs.B;

        // We want to compute
        //
        // (a + bi)*(c + di) = (a*c - b*d) + (a*d + b*c)i
        //
        // Use Karatsuba's trick: note that
        //
        // (b - a)*(c - d) = (b*c + a*d) - a*c - b*d
        //
        // so (a*d + b*c) = (b-a)*(c-d) + a*c + b*d.
        //
        let ac = a * c;                               // = a*c*R*R
        let bd = b * d;                               // = b*d*R*R
        let b_minus_a = b - a;                        // = (b-a)*R
        let c_minus_d = c - d;                        // = (c-d)*R
        
        let mut ad_plus_bc = &b_minus_a * &c_minus_d; // = (b-a)*(c-d)*R*R
        ad_plus_bc += &ac;                            // = ((b-a)*(c-d) + a*c)*R*R
        ad_plus_bc += &bd;                            // = ((b-a)*(c-d) + a*c + b*d)*R*R
        let _b = ad_plus_bc.reduce();                 // = (a*d + b*c)*R mod p

        let ac_minus_bd = &ac - &bd;                  // = (a*c - b*d)*R*R
        let _a = ac_minus_bd.reduce();                // = (a*c - b*d)*R mod p  

        ExtensionFieldElement{
            A: _a,
            B: _b
        }
    }
}

impl <'a> Neg for &'a ExtensionFieldElement {
    type Output = ExtensionFieldElement;
    #[inline(always)]
    fn neg(self) -> ExtensionFieldElement {
        let zero = ExtensionFieldElement::zero();
        let result = &zero - (self as &ExtensionFieldElement);
        result
    }
}

impl ConditionallySelectable for ExtensionFieldElement {
    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        ExtensionFieldElement{
            A: Fp751Element::conditional_select(&a.A, &b.A, choice),
            B: Fp751Element::conditional_select(&a.B, &b.B, choice)
        }
    }

    fn conditional_swap(a: &mut Self, b: &mut Self, choice: Choice) {
        Fp751Element::conditional_swap(&mut a.A, &mut b.A, choice);
        Fp751Element::conditional_swap(&mut a.B, &mut b.B, choice);
    }
}

impl Debug for ExtensionFieldElement {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "ExtensionFieldElement(A: {:?}\nB: {:?})", &self.A.0[..], &self.B.0[..])
    }
}

#[cfg(test)]
pub struct ExtensionFieldElementDist;

#[cfg(test)]
impl Arbitrary for ExtensionFieldElement {
    fn arbitrary(g: &mut Gen) -> ExtensionFieldElement {
        let a = Fp751Element::arbitrary(g);
        let b = Fp751Element::arbitrary(g);
        ExtensionFieldElement{ A: a, B: b }
    }
}

impl ExtensionFieldElement {
    /// Construct a zero `ExtensionFieldElement`.
    #[inline(always)]
    pub fn zero() -> ExtensionFieldElement {
        ExtensionFieldElement{
            A: Fp751Element::zero(),
            B: Fp751Element::zero(),
        }
    }

    /// Construct a one `ExtensionFieldElement`.
    #[inline(always)]
    pub fn one() -> ExtensionFieldElement {
        ExtensionFieldElement{
            A: Fp751Element([0x249ad, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x83100000, 0x375c6c66, 0x5527b1e4, 0x3f4f24d0, 0x697797bf, 0xac5c4e2e, 0xc89db7b2, 0xd2076956, 0x4ca4b439, 0x7512c7e9, 0x10f7926c, 0x24bce5e2, 0x2d5b]),
            B: Fp751Element([0_u32; FP751_NUM_WORDS]),
        }
    }

    /// Set output to `1/x`.
    pub fn inv(&self) -> ExtensionFieldElement {
        let a = &self.A;
        let b = &self.B;

        // We want to compute
        //
        //    1          1     (a - bi)	    (a - bi)
        // -------- = -------- -------- = -----------
        // (a + bi)   (a + bi) (a - bi)   (a^2 + b^2)
        //
        // Letting c = 1/(a^2 + b^2), this is
        //
        // 1/(a+bi) = a*c - b*ci.
        //
        let mut asq = a * a;           // = a*a*R*R
        let bsq = b * b;               // = b*b*R*R
        asq = &asq + &bsq;             // = (a^2 + b^2)*R*R
        let mut asq_plus_bsq = PrimeFieldElement::zero();
        asq_plus_bsq.A = asq.reduce(); // = (a^2 + b^2)*R mod p
        // Now asq_plus_bsq = a^2 + b^2

        let asq_plus_bsq_inv = asq_plus_bsq.inv();
        let c = &asq_plus_bsq_inv.A;

        let ac = a * c;
        let _a = ac.reduce();

        let mut minus_b = Fp751Element::zero();
        minus_b = &minus_b - &b;
        let minus_bc = &minus_b * &c;
        let _b = minus_bc.reduce();

        ExtensionFieldElement{
            A: _a,
            B: _b
        }
    }

    // Set (y1, y2, y3)  = (1/x1, 1/x2, 1/x3).
    //
    // All xi, yi must be distinct.
    pub fn batch3_inv(x1: &ExtensionFieldElement, x2: &ExtensionFieldElement, x3: &ExtensionFieldElement) ->
                 (ExtensionFieldElement, ExtensionFieldElement, ExtensionFieldElement)
    {
        let x1x2 = x1 * x2;     // x1*x2
        let mut t = &x1x2 * x3;
        t = t.inv();            // 1/(x1*x2*x3)
        let y1 = &t * x2;
        let _y1 = &y1 * x3;     // 1/x1
        let y2 = &t * x1;
        let _y2 = &y2 * x3;     // 1/x2
        let _y3 = &t * &x1x2;   // 1/x3

        (_y1, _y2, _y3)
    }

    /// Set the output to `x^2`.
    pub fn square(&self) -> ExtensionFieldElement {
        let a = &self.A;
        let b = &self.B;

        // We want to compute
	    //
	    // (a + bi)*(a + bi) = (a^2 - b^2) + 2abi
        //
        let a2 = a + a;        // = a*R + a*R = 2*a*R
        let a_plus_b = a + b;  // = a*R + b*R = (a+b)*R
        let a_minus_b = a - b; // = a*R - b*R = (a-b)*R

        let asq_minus_bsq = &a_plus_b * &a_minus_b; // = (a+b)*(a-b)*R*R = (a^2 - b^2)*R*R
        let ab2 = &a2 * b;                          // = 2*a*b*R*R                       

        let _a = asq_minus_bsq.reduce(); // = (a^2 - b^2)*R mod p
        let _b = ab2.reduce();           // = 2*a*b*R mod p

        ExtensionFieldElement{
            A: _a,
            B: _b
        }
    }

    /// Returns true if both sides are equal. Takes variable time.
    #[inline(always)]
    pub fn vartime_eq(&self, _rhs: &ExtensionFieldElement) -> bool {
        (&self.A == &_rhs.A) && (&self.B == &_rhs.B)
    }

    /// Convert the input to wire format.
    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 188] {
        let mut bytes = [0u8; 188];
        bytes[0..94].clone_from_slice(&self.A.to_bytes());
        bytes[94..188].clone_from_slice(&self.B.to_bytes());
        bytes
    }

    /// Read 188 bytes into the given `ExtensionFieldElement`.
    #[inline(always)]
    pub fn from_bytes(bytes: &[u8]) -> ExtensionFieldElement {
        assert!(bytes.len() >= 188, "Too short input to ExtensionFieldElement from_bytes, expected 188 bytes");
        let a = Fp751Element::from_bytes(&bytes[0..94]);
        let b = Fp751Element::from_bytes(&bytes[94..188]);
        ExtensionFieldElement{ A: a, B: b }
    }
}

//-----------------------------------------------------------------------------//
//                             Prime Field                                     //
//-----------------------------------------------------------------------------//

/// Represents an element of the prime field `F_p`.
#[allow(non_snake_case)]
#[derive(Copy, Clone, PartialEq)]
pub struct PrimeFieldElement {
    /// This field element is in Montgomery form, so that the value `A` is
	/// represented by `aR mod p`.
    pub A: Fp751Element
}

impl<'b> AddAssign<&'b PrimeFieldElement> for PrimeFieldElement {
    #[inline(always)]
    fn add_assign(&mut self, _rhs: &'b PrimeFieldElement) {
        let result = (self as &PrimeFieldElement) + _rhs;
        self.A = result.A;
    }
}

impl<'a, 'b> Add<&'b PrimeFieldElement> for &'a PrimeFieldElement {
    type Output = PrimeFieldElement;
    #[inline(always)]
    fn add(self, _rhs: &'b PrimeFieldElement) -> PrimeFieldElement {
        let a = &self.A + &_rhs.A;
        PrimeFieldElement{ A: a }
    }
}

impl <'b> SubAssign<&'b PrimeFieldElement> for PrimeFieldElement {
    #[inline(always)]
    fn sub_assign(&mut self, _rhs: &'b PrimeFieldElement) {
        let result = (self as &PrimeFieldElement) - _rhs;
        self.A = result.A;
    }
}

impl<'a, 'b> Sub<&'b PrimeFieldElement> for &'a PrimeFieldElement {
    type Output = PrimeFieldElement;
    #[inline(always)]
    fn sub(self, _rhs: &'b PrimeFieldElement) -> PrimeFieldElement {
        let a = &self.A - &_rhs.A;
        PrimeFieldElement{ A: a }
    }
}

impl<'b> MulAssign<&'b PrimeFieldElement> for PrimeFieldElement {
    #[inline(always)]
    fn mul_assign(&mut self, _rhs: &'b PrimeFieldElement) {
        let result = (self as &PrimeFieldElement) * _rhs;
        self.A = result.A;
    }
}

impl<'a, 'b> Mul<&'b PrimeFieldElement> for &'a PrimeFieldElement {
    type Output = PrimeFieldElement;
    #[inline(always)]
    fn mul(self, _rhs: &'b PrimeFieldElement) -> PrimeFieldElement {
        // Alias self, _rhs for more readable formulas.
        let a = &self.A;      // = a*R
        let b = &_rhs.A;      // = b*R
        let ab = a * b;       // = a*b*R*R
        let _a = ab.reduce(); // = a*b*R mod p
        
        PrimeFieldElement{ A: _a }
    }
}

impl <'a> Neg for &'a PrimeFieldElement {
    type Output = PrimeFieldElement;
    #[inline(always)]
    fn neg(self) -> PrimeFieldElement {
        let zero = PrimeFieldElement::zero();
        let result = &zero - (self as &PrimeFieldElement);
        result
    }
}

impl ConditionallySelectable for PrimeFieldElement {
    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        PrimeFieldElement{
            A: Fp751Element::conditional_select(&a.A, &b.A, choice)
        }
    }

    fn conditional_swap(a: &mut Self, b: &mut Self, choice: Choice) {
        Fp751Element::conditional_swap(&mut a.A, &mut b.A, choice);
    }
}

impl Debug for PrimeFieldElement {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "PrimeFieldElement(A: {:?})", &self.A.0[..])
    }
}

#[cfg(test)]
impl Arbitrary for PrimeFieldElement {
    fn arbitrary(g: &mut Gen) -> PrimeFieldElement {
        let a = Fp751Element::arbitrary(g);
        PrimeFieldElement{ A: a }
    }
}

impl PrimeFieldElement {
    /// Construct a zero `PrimeFieldElement`.
    #[inline(always)]
    pub fn zero() -> PrimeFieldElement {
        PrimeFieldElement{
            A: Fp751Element::zero(),
        }
    }

    /// Construct a one `PrimeFieldElement`.
    pub fn one() -> PrimeFieldElement {
        PrimeFieldElement{
            A: Fp751Element([0x249ad, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x83100000, 0x375c6c66, 0x5527b1e4, 0x3f4f24d0, 0x697797bf, 0xac5c4e2e, 0xc89db7b2, 0xd2076956, 0x4ca4b439, 0x7512c7e9, 0x10f7926c, 0x24bce5e2, 0x2d5b]),
        }
    }

    /// Set the output to `x^2`.
    #[inline(always)]
    pub fn square(&self) -> PrimeFieldElement {
        let a = &self.A;      // = a*R
        let b = &self.A;      // = b*R
        let ab = a * b;       // = a*b*R*R
        let _a = ab.reduce(); // = a*b*R mod p

        PrimeFieldElement{ A: _a }
    }

    /// Raise self to `2^(2^k)`-th power, for `k >= 1`, by repeated squarings.
    fn pow2k(&self, k: u8) -> PrimeFieldElement {
        let mut result = self.square();
        for _ in 1..k { result = result.square(); }
        result
    }

    /// Set output to `x^((p-3)/4)`. If `x` is square, this is `1/sqrt(x)`.
    fn p34(&self) -> PrimeFieldElement {
        // Sliding-window strategy computed with Sage, awk, sed, and tr.
        //
        // This performs sum(powStrategy) = 744 squarings and len(mulStrategy)
        // = 137 multiplications, in addition to 1 squaring and 15
        // multiplications to build a lookup table.
        //
        // In total this is 745 squarings, 152 multiplications.  Since squaring
        // is not implemented for the prime field, this is 897 multiplications
        // in total.
        let pow_strategy: [u8; 137] = [5, 7, 6, 2, 10, 4, 6, 9, 8, 5, 9, 4, 7, 5, 5, 4, 8, 3, 9, 5, 5, 4, 10, 4, 6, 6, 6, 5, 8, 9, 3, 4, 9, 4, 5, 6, 6, 2, 9, 4, 5, 5, 5, 7, 7, 9, 4, 6, 4, 8, 5, 8, 6, 6, 2, 9, 7, 4, 8, 8, 8, 4, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2];
        let mul_strategy: [u8; 137] = [31, 23, 21, 1, 31, 7, 7, 7, 9, 9, 19, 15, 23, 23, 11, 7, 25, 5, 21, 17, 11, 5, 17, 7, 11, 9, 23, 9, 1, 19, 5, 3, 25, 15, 11, 29, 31, 1, 29, 11, 13, 9, 11, 27, 13, 19, 15, 31, 3, 29, 23, 31, 25, 11, 1, 21, 19, 15, 15, 21, 29, 13, 23, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 3];
        let initial_mul: u8 = 27;

        // Build a lookup table of odd multiples of x.
        let mut lookup = [PrimeFieldElement::zero(); 16];
        let xx: &PrimeFieldElement = &self.square(); // Set xx = x^2
        lookup[0] = *self;

        for i in 1..16 {
            lookup[i as usize] = &lookup[(i-1) as usize] * xx;
        }
        // Now lookup = {x, x^3, x^5, ... }
	    // so that lookup[i] = x^{2*i + 1}
	    // so that lookup[k/2] = x^k, for odd k
        let mut result = lookup[(initial_mul / 2) as usize];
        for i in 0..137 {
            result = result.pow2k(pow_strategy[i]);
            result = &result * &lookup[(mul_strategy[i] / 2) as usize];
        }
        result
    }

    /// Set output to `sqrt(x)`, if x is a square. If `x` is nonsquare output is undefined.
    fn sqrt(&self) -> PrimeFieldElement {
        let mut result = self.p34(); // result = (y^2)^((p-3)/4) = y^((p-3)/2)
        result = &result * self;     // result = y^2 * y^((p-3)/2) = y^((p+1)/2)
        // Now result^2 = y^(p+1) = y^2 = x, so result = sqrt(x).
        result
    }

    /// Set output to `1/x`.
    pub fn inv(&self) -> PrimeFieldElement {
        let mut result = self.square(); // result = x^2
        result = result.p34();          // result = (x^2)^((p-3)/4) = x^((p-3)/2)
        result = result.square();       // result = x^(p-3)
        result = &result * self;        // result = x^(p-2)
        result
    }

    /// Returns true if both sides are equal. Takes variable time.
    #[inline(always)]
    pub fn vartime_eq(&self, _rhs: &PrimeFieldElement) -> bool {
        &self.A == &_rhs.A
    }
}

//-----------------------------------------------------------------------------//
//                              Internals                                      //
//-----------------------------------------------------------------------------//

impl<'b> AddAssign<&'b Fp751Element> for Fp751Element {
    #[inline(always)]
    fn add_assign(&mut self, _rhs: &'b Fp751Element) {
        let result = (self as &Fp751Element) + _rhs;
        self.0 = result.0
    }
}

impl<'a, 'b> Add<&'b Fp751Element> for &'a Fp751Element {
    type Output = Fp751Element;
    #[inline(always)]
    fn add(self, _rhs: &'b Fp751Element) -> Fp751Element {
        let mut result = Fp751Element::zero();
        fpadd751(&self, _rhs, &mut result);
        result
    }
}

impl <'b> SubAssign<&'b Fp751Element> for Fp751Element {
    #[inline(always)]
    fn sub_assign(&mut self, _rhs: &'b Fp751Element) {
        let result = (self as &Fp751Element) - _rhs;
        self.0 = result.0
    }
}

impl<'a, 'b> Sub<&'b Fp751Element> for &'a Fp751Element {
    type Output = Fp751Element;
    #[inline(always)]
    fn sub(self, _rhs: &'b Fp751Element) -> Fp751Element {
        let mut result = Fp751Element::zero();
        fpsub751(&self, _rhs, &mut result);
        result
    }
}

impl<'a, 'b> Mul<&'b Fp751Element> for &'a Fp751Element {
    type Output = Fp751X2;
    #[inline(always)]
    fn mul(self, _rhs: &'b Fp751Element) -> Fp751X2 {
        let mut result = Fp751X2::zero();
        mul751(&self, _rhs, &mut result); // = a*c*R*R
        result
    }
}

impl <'a> Neg for &'a Fp751Element {
    type Output = Fp751Element;
    #[inline(always)]
    fn neg(self) -> Fp751Element {
        let zero = Fp751Element::zero();
        let result = &zero - (self as &Fp751Element);
        result
    }
}

impl Eq for Fp751Element {}
impl PartialEq for Fp751Element {
    /// Test equality between two `Fp751Element`s.
    /// 
    /// # Warning
    /// 
    /// This comparison is *not* constant time.
    fn eq(&self, other: &Fp751Element) -> bool {
        let mut _self = *self;
        let mut _other = *other;

        _self = _self.strong_reduce();
        _other = _other.strong_reduce();

        let mut eq: bool = true;
        for i in 0..FP751_NUM_WORDS {
            eq = (_self.0[i] == _other.0[i]) && eq;
        }
        eq
    }
}

impl ConstantTimeEq for Fp751Element {
    /// Test equality between two `Fp751Element`s.
    ///
    /// # Returns
    ///
    /// `1u8` if the two `Fp751Element`s are equal, and `0u8` otherwise.
    fn ct_eq(&self, other: &Fp751Element) -> Choice {
        let self_bytes = self.to_bytes();
        let other_bytes = other.to_bytes();
        let len = self_bytes.len();

        // Short-circuit on the *lengths* of the slices, not their
        // contents.
        if len != other_bytes.len() {
            return Choice::from(0);
        }

        // This loop shouldn't be shortcircuitable, since the compiler
        // shouldn't be able to reason about the value of the `u8`
        // unwrapped from the `ct_eq` result.
        let mut x = 1u8;
        for (selfi, otheri) in self_bytes.iter().zip(other_bytes.iter()) {
            x &= selfi.ct_eq(otheri).unwrap_u8();
        }

        x.into()
    }
}

// #[cfg(test)]
// impl Arbitrary for Fp751Element {
//     fn arbitrary(g: &mut Gen) -> Fp751Element {
//         Fp751Element::arbitrary(g);
//     }
// }

impl Fp751Element {
    /// Reduce a field element in `[0, 2*p)` to one in `[0,p)`.
    #[inline(always)]
    pub fn strong_reduce(&self) -> Fp751Element {
        let mut _self = *self;
        srdc751(&mut _self);
        _self
    }
}

impl<'b> AddAssign<&'b Fp751X2> for Fp751X2 {
    #[inline(always)]
    fn add_assign(&mut self, _rhs: &'b Fp751X2) {
        let result = (self as &Fp751X2) + _rhs;
        self.0 = result.0
    }
}

impl<'a, 'b> Add<&'b Fp751X2> for &'a Fp751X2 {
    type Output = Fp751X2;
    #[inline(always)]
    fn add(self, _rhs: &'b Fp751X2) -> Fp751X2 {
        let mut result = Fp751X2::zero();
        mp_add751x2(&self, _rhs, &mut result);
        result
    }
}

impl <'b> SubAssign<&'b Fp751X2> for Fp751X2 {
    #[inline(always)]
    fn sub_assign(&mut self, _rhs: &'b Fp751X2) {
        let result = (self as &Fp751X2) - _rhs;
        self.0 = result.0
    }
}

impl<'a, 'b> Sub<&'b Fp751X2> for &'a Fp751X2 {
    type Output = Fp751X2;
    #[inline(always)]
    fn sub(self, _rhs: &'b Fp751X2) -> Fp751X2 {
        let mut result = Fp751X2::zero();
        mp_sub751x2(&self, _rhs, &mut result);
        result
    }
}

impl Fp751X2 {
    /// Perform Montgomery reduction, `x R^{-1} (mod p)`.
    #[inline(always)]
    pub fn reduce(&self) -> Fp751Element {
        let mut result = Fp751Element::zero();
        rdc751(self, &mut result);
        result
    }
}

#[inline(always)]
pub fn checklt238(scalar: &[u8; 48], result: &mut u32) {
    crate::sidhp751::fp::checklt238(scalar, result);
}

#[inline(always)]
pub fn mulby3(scalar: &mut [u8; 48]) {
    crate::sidhp751::fp::mulby3(scalar);
}

#[cfg(test)]
mod test {
    use super::*;
    use quickcheck::QuickCheck;

    const SCALE_FACTOR: u8 = 3;
    const MAX_TESTS: u64 = 1 << (10 + SCALE_FACTOR);

    #[test]
    fn one_extension_field_to_byte() {
        let one = &ExtensionFieldElement::one();
        let bytes = one.to_bytes();

        assert_eq!(bytes[0], 1);

        for i in 1..188 {
            assert_eq!(bytes[i], 0);
        }
    }
    
    #[test]
    fn extension_field_element_to_bytes_round_trip() {
        fn round_trips(x: ExtensionFieldElement) -> bool {
            let bytes = x.to_bytes();
            let x_prime = ExtensionFieldElement::from_bytes(&bytes);
            x.vartime_eq(&x_prime)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(round_trips as fn(ExtensionFieldElement) -> bool);
    }

    #[test]
    fn extension_field_element_mul_distributes_over_add() {
        fn mul_distributes_over_add(x: ExtensionFieldElement, y: ExtensionFieldElement, z: ExtensionFieldElement) -> bool {
            // Compute t1 = (x+y)*z
            let t1 = &(&x + &y) * &z;
            // Compute t2 = x*z + y*z
            let t2 = &(&x * &z) + &(&y * &z);

            t1.vartime_eq(&t2)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(mul_distributes_over_add as fn(ExtensionFieldElement, ExtensionFieldElement, ExtensionFieldElement) -> bool);
    }

    #[test]
    fn extension_field_element_mul_is_associative() {
        fn is_associative(x: ExtensionFieldElement, y: ExtensionFieldElement, z: ExtensionFieldElement) -> bool {
            // Compute t1 = (x*y)*z
            let t1 = &(&x * &y) * &z;
            // Compute t2 = (y*z)*x
            let t2 = &(&y * &z) * &x;

            t1.vartime_eq(&t2)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(is_associative as fn(ExtensionFieldElement, ExtensionFieldElement, ExtensionFieldElement) -> bool);
    }

    #[test]
    fn extension_field_element_square_matches_mul() {
        fn square_matches_mul(x: ExtensionFieldElement) -> bool {
            // Compute t1 = (x*x)
            let t1 = &x * &x;
            // Compute t2 = x^2
            let t2 = x.square();

            t1.vartime_eq(&t2)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(square_matches_mul as fn(ExtensionFieldElement) -> bool);
    }

    #[test]
    fn extension_field_element_inv() {
        fn inverse(x: ExtensionFieldElement) -> bool {
            let mut z = x.inv();
            // Now z = (1/x), so (z * x) * x == x
            z = &(&z * &x) * &x;

            z.vartime_eq(&x)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(inverse as fn(ExtensionFieldElement) -> bool);
    }

    #[test]
    fn extension_field_element_batch3_inv() {
        fn batch_inverse(x1: ExtensionFieldElement, x2: ExtensionFieldElement, x3: ExtensionFieldElement) -> bool {
            let x1_inv = x1.inv();
            let x2_inv = x2.inv();
            let x3_inv = x3.inv();

            let (y1, y2, y3) = ExtensionFieldElement::batch3_inv(&x1, &x2, &x3);

            y1.vartime_eq(&x1_inv) && y2.vartime_eq(&x2_inv) && y3.vartime_eq(&x3_inv)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(batch_inverse as fn(ExtensionFieldElement, ExtensionFieldElement, ExtensionFieldElement) -> bool);
    }

    #[test]
    fn prime_field_element_inv() {
        fn inverse(x: PrimeFieldElement) -> bool {
            let mut z = x.inv();
            // Now z = (1/x), so (z * x) * x == x
            z = &(&z * &x) * &x;

            z.vartime_eq(&x)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(inverse as fn(PrimeFieldElement) -> bool);
    }

    #[test]
    fn prime_field_element_sqrt() {
        fn square_root(x: PrimeFieldElement) -> bool {
            // Construct y = x^2 so we're sure y is square.
            let y = x.square();
            let mut z = y.sqrt();
            // Now z = sqrt(y), so z^2 == y
            z = z.square();

            z.vartime_eq(&y)
        }
        QuickCheck::new().max_tests(MAX_TESTS)
                         .quickcheck(square_root as fn(PrimeFieldElement) -> bool);
    }

    #[test]
    fn fp751_element_conditional_swap() {
        let one = Fp751Element([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);
        let two = Fp751Element([2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2]);

        let mut x = one;
        let mut y = two;

        Fp751Element::conditional_swap(&mut x, &mut y, 0.into());
        assert_eq!(x, one); 
        assert_eq!(y, two);

        Fp751Element::conditional_swap(&mut x, &mut y, 1.into());
        assert_eq!(x, two);
        assert_eq!(y, one);
    }
}
