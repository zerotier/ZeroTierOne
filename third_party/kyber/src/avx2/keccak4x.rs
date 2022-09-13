// Macro and function code structure is the work of Marek Kotewicz
// plus contributors to the tiny-keccak crate licensed under
// Creative Commons CC0 1.0 Universal. Thankyou.
// https://github.com/debris/tiny-keccak

// Copyright 2020-2021 Mitchell Berry
// Licensed under the Apache License, Version 2.0

// Drop-in Rust replacement for KeccakP-1600-times4 function for
// the eXtended Keccak Code Package https://github.com/XKCP/XKCP

// Test vectors taken from:
// https://github.com/XKCP/XKCP/blob/master/tests/TestVectors/KeccakF-1600-IntermediateValues.txt

use core::arch::x86_64::*;

#[repr(C, align(32))]
union RC_Data {
    vecs: [__m256i; 24],
    u: [u64; 96],
}

#[repr(C, align(32))]
union Temp {
    pub vec: __m256i,
    pub u: [u64; 4],
}

const RHO: [u32; 24] = [
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44,
];

const PI: [usize; 24] = [
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1,
];

// Set __mm256i constants with a union
const RC_X4: RC_Data = RC_Data {
    u: [
        0x0000000000000001,
        0x0000000000000001,
        0x0000000000000001,
        0x0000000000000001,
        0x0000000000008082,
        0x0000000000008082,
        0x0000000000008082,
        0x0000000000008082,
        0x800000000000808a,
        0x800000000000808a,
        0x800000000000808a,
        0x800000000000808a,
        0x8000000080008000,
        0x8000000080008000,
        0x8000000080008000,
        0x8000000080008000,
        0x000000000000808b,
        0x000000000000808b,
        0x000000000000808b,
        0x000000000000808b,
        0x0000000080000001,
        0x0000000080000001,
        0x0000000080000001,
        0x0000000080000001,
        0x8000000080008081,
        0x8000000080008081,
        0x8000000080008081,
        0x8000000080008081,
        0x8000000000008009,
        0x8000000000008009,
        0x8000000000008009,
        0x8000000000008009,
        0x000000000000008a,
        0x000000000000008a,
        0x000000000000008a,
        0x000000000000008a,
        0x0000000000000088,
        0x0000000000000088,
        0x0000000000000088,
        0x0000000000000088,
        0x0000000080008009,
        0x0000000080008009,
        0x0000000080008009,
        0x0000000080008009,
        0x000000008000000a,
        0x000000008000000a,
        0x000000008000000a,
        0x000000008000000a,
        0x000000008000808b,
        0x000000008000808b,
        0x000000008000808b,
        0x000000008000808b,
        0x800000000000008b,
        0x800000000000008b,
        0x800000000000008b,
        0x800000000000008b,
        0x8000000000008089,
        0x8000000000008089,
        0x8000000000008089,
        0x8000000000008089,
        0x8000000000008003,
        0x8000000000008003,
        0x8000000000008003,
        0x8000000000008003,
        0x8000000000008002,
        0x8000000000008002,
        0x8000000000008002,
        0x8000000000008002,
        0x8000000000000080,
        0x8000000000000080,
        0x8000000000000080,
        0x8000000000000080,
        0x000000000000800a,
        0x000000000000800a,
        0x000000000000800a,
        0x000000000000800a,
        0x800000008000000a,
        0x800000008000000a,
        0x800000008000000a,
        0x800000008000000a,
        0x8000000080008081,
        0x8000000080008081,
        0x8000000080008081,
        0x8000000080008081,
        0x8000000000008080,
        0x8000000000008080,
        0x8000000000008080,
        0x8000000000008080,
        0x0000000080000001,
        0x0000000080000001,
        0x0000000080000001,
        0x0000000080000001,
        0x8000000080008008,
        0x8000000080008008,
        0x8000000080008008,
        0x8000000080008008,
    ],
};

macro_rules! unroll5 {
    ($var:ident, $body:block) => {{
        const $var: usize = 0;
        $body;
    }
    {
        const $var: usize = 1;
        $body;
    }
    {
        const $var: usize = 2;
        $body;
    }
    {
        const $var: usize = 3;
        $body;
    }
    {
        const $var: usize = 4;
        $body;
    }};
}

macro_rules! unroll24 {
    ($var: ident, $body: block) => {{
        const $var: usize = 0;
        $body;
    }
    {
        const $var: usize = 1;
        $body;
    }
    {
        const $var: usize = 2;
        $body;
    }
    {
        const $var: usize = 3;
        $body;
    }
    {
        const $var: usize = 4;
        $body;
    }
    {
        const $var: usize = 5;
        $body;
    }
    {
        const $var: usize = 6;
        $body;
    }
    {
        const $var: usize = 7;
        $body;
    }
    {
        const $var: usize = 8;
        $body;
    }
    {
        const $var: usize = 9;
        $body;
    }
    {
        const $var: usize = 10;
        $body;
    }
    {
        const $var: usize = 11;
        $body;
    }
    {
        const $var: usize = 12;
        $body;
    }
    {
        const $var: usize = 13;
        $body;
    }
    {
        const $var: usize = 14;
        $body;
    }
    {
        const $var: usize = 15;
        $body;
    }
    {
        const $var: usize = 16;
        $body;
    }
    {
        const $var: usize = 17;
        $body;
    }
    {
        const $var: usize = 18;
        $body;
    }
    {
        const $var: usize = 19;
        $body;
    }
    {
        const $var: usize = 20;
        $body;
    }
    {
        const $var: usize = 21;
        $body;
    }
    {
        const $var: usize = 22;
        $body;
    }
    {
        const $var: usize = 23;
        $body;
    }};
}

#[allow(unused_assignments, non_upper_case_globals)]
pub fn f1600_x4(a: &mut [__m256i]) {
    unsafe {
        for i in 0..24 {
            let mut array = [_mm256_setzero_si256(); 5];

            // Theta
            unroll5!(x, {
                unroll5!(y, {
                    array[x] = _mm256_xor_si256(array[x], a[5 * y + x]);
                });
            });

            unroll5!(x, {
                unroll5!(y, {
                    let t1 = array[(x + 4) % 5];
                    let mut t2 = Temp { vec: array[(x + 1) % 5] };
                    for i in 0..4 {
                        t2.u[i] = t2.u[i].rotate_left(1);
                    }
                    a[5 * y + x] = _mm256_xor_si256(a[5 * y + x], _mm256_xor_si256(t1, t2.vec));
                });
            });

            // Rho and pi
            let mut last = a[1];
            unroll24!(x, {
                array[0] = a[PI[x]];
                let mut temp_last = Temp { vec: last };
                for i in 0..4 {
                    temp_last.u[i] = temp_last.u[i].rotate_left(RHO[x]);
                }
                a[PI[x]] = temp_last.vec;
                last = array[0];
            });

            // Chi
            unroll5!(y_step, {
                let y = 5 * y_step;

                unroll5!(x, {
                    array[x] = a[y + x];
                });

                unroll5!(x, {
                    let t1 = array[(x + 1) % 5];
                    let t2 = array[(x + 2) % 5];
                    let tmp = _mm256_xor_si256(array[x], _mm256_andnot_si256(t1, t2));
                    a[y + x] = tmp;
                });
            });
            a[0] = _mm256_xor_si256(a[0], RC_X4.vecs[i]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    const PLEN: usize = 25;
    // Test vectors from XKCP
    // https://github.com/XKCP/XKCP/blob/master/tests/TestVectors/KeccakF-1600-IntermediateValues.txt
    #[test]
    fn known_vectors() {
        let vec1: [u64; 25] = [
            0xF1258F7940E1DDE7,
            0x84D5CCF933C0478A,
            0xD598261EA65AA9EE,
            0xBD1547306F80494D,
            0x8B284E056253D057,
            0xFF97A42D7F8E6FD4,
            0x90FEE5A0A44647C4,
            0x8C5BDA0CD6192E76,
            0xAD30A6F71B19059C,
            0x30935AB7D08FFC64,
            0xEB5AA93F2317D635,
            0xA9A6E6260D712103,
            0x81A57C16DBCF555F,
            0x43B831CD0347C826,
            0x01F22F1A11A5569F,
            0x05E5635A21D9AE61,
            0x64BEFEF28CC970F2,
            0x613670957BC46611,
            0xB87C5A554FD00ECB,
            0x8C3EE88A1CCF32C8,
            0x940C7922AE3A2614,
            0x1841F924A2C509E4,
            0x16F53526E70465C2,
            0x75F644E97F30A13B,
            0xEAF1FF7B5CECA249,
        ];

        let vec2: [u64; 25] = [
            0x2D5C954DF96ECB3C,
            0x6A332CD07057B56D,
            0x093D8D1270D76B6C,
            0x8A20D9B25569D094,
            0x4F9C4F99E5E7F156,
            0xF957B9A2DA65FB38,
            0x85773DAE1275AF0D,
            0xFAF4F247C3D810F7,
            0x1F1B9EE6F79A8759,
            0xE4FECC0FEE98B425,
            0x68CE61B6B9CE68A1,
            0xDEEA66C4BA8F974F,
            0x33C43D836EAFB1F5,
            0xE00654042719DBD9,
            0x7CF8A9F009831265,
            0xFD5449A6BF174743,
            0x97DDAD33D8994B40,
            0x48EAD5FC5D0BE774,
            0xE3B8C8EE55B7B03C,
            0x91A0226E649E42E9,
            0x900E3129E7BADD7B,
            0x202A9EC5FAA3CCE8,
            0x5B3402464E1C3DB6,
            0x609F4E62A44C1059,
            0x20D06CD26A8FBF5C,
        ];

        // repeat values to check all lanes
        let tvec1 = expand(vec1);
        let tvec2 = expand(vec2);

        unsafe {
            let mut data = Data { u: [0u64; 100] };
            f1600_x4(&mut data.lanes);
            assert_eq!(&data.u, &tvec1);
            f1600_x4(&mut data.lanes);
            assert_eq!(data.u, tvec2);
        }
    }
    #[repr(C)]
    pub union Data {
        pub lanes: [__m256i; PLEN],
        pub u: [u64; PLEN * 4],
    }

    // [0,1...] expands to [0,0,0,0,1,1,1,1...]
    fn expand(vec: [u64; PLEN]) -> [u64; 100] {
        let mut out = [0u64; 100];
        for (i, u) in vec.iter().enumerate() {
            out[i * 4..][..4].copy_from_slice(&[*u; 4]);
        }
        out
    }
}
