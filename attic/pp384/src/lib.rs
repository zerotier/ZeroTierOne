use std::cmp::Ordering;

const ECC_BYTES: usize = 48;
const ECC_QUADS: usize = ECC_BYTES / 8;
const MAX_TRIES: usize = 4096;

const P384_P_48: [u64; ECC_QUADS] = [
    0x00000000FFFFFFFF,
    0xFFFFFFFF00000000,
    0xFFFFFFFFFFFFFFFE,
    0xFFFFFFFFFFFFFFFF,
    0xFFFFFFFFFFFFFFFF,
    0xFFFFFFFFFFFFFFFF,
];
const P384_B_48: [u64; ECC_QUADS] = [
    0x2A85C8EDD3EC2AEF,
    0xC656398D8A2ED19D,
    0x0314088F5013875A,
    0x181D9C6EFE814112,
    0x988E056BE3F82D19,
    0xB3312FA7E23EE7E4,
];
const P384_G_48: [[u64; ECC_QUADS]; 2] = [
    [
        0x3A545E3872760AB7,
        0x5502F25DBF55296C,
        0x59F741E082542A38,
        0x6E1D3B628BA79B98,
        0x8EB1C71EF320AD74,
        0xAA87CA22BE8B0537,
    ],
    [
        0x7A431D7C90EA0E5F,
        0x0A60B1CE1D7E819D,
        0xE9DA3113B5F0B8C0,
        0xF8F41DBD289A147C,
        0x5D9E98BF9292DC29,
        0x3617DE4A96262C6F,
    ],
];
const P384_N_48: [u64; ECC_QUADS] = [
    0xECEC196ACCC52973,
    0x581A0DB248B0A77A,
    0xC7634D81F4372DDF,
    0xFFFFFFFFFFFFFFFF,
    0xFFFFFFFFFFFFFFFF,
    0xFFFFFFFFFFFFFFFF,
];

fn pvli_clear<const P: usize>(pvli: &mut [[u64; ECC_QUADS]; P]) {
    pvli.iter_mut().for_each(|vli| vli.fill(0));
}

fn pvli_is_zero<const P: usize>(pvli: &[[u64; ECC_QUADS]; P]) -> [bool; P] {
    pvli.map(|vli| vli.iter().all(|i| *i == 0))
}

fn pvli_test_bit<const P: usize>(pvli: &[[u64; ECC_QUADS]; P], bit: u32) -> [bool; P] {
    pvli.map(|vli| (vli[(bit as usize) / 64] & 1u64.wrapping_shl(bit % 64)) != 0)
}

fn pvli_num_digits<const P: usize>(pvli: &[[u64; ECC_QUADS]; P]) -> [usize; P] {
    pvli.map(|vli| {
        let mut i = (ECC_QUADS as isize) - 1;
        while i >= 0 && vli[i as usize] == 0 {
            i -= 1;
        }
        (i + 1) as usize
    })
}

fn pvli_num_bits<const P: usize>(pvli: &[[u64; ECC_QUADS]; P]) -> [usize; P] {
    pvli.map(|vli| {
        let mut bits = 0;
        for i in 0..ECC_QUADS {
            bits += vli[i].count_ones();
        }
        bits as usize
    })
}

fn pvli_cmp<const P: usize>(left: &[[u64; ECC_QUADS]; P], right: &[[u64; ECC_QUADS]; P]) -> [Ordering; P] {
    let mut r = [Ordering::Equal; P];
    for p in 0..P {
        let mut i = (ECC_QUADS as isize) - 1;
        while i >= 0 {
            if left[p][i as usize] > right[p][i as usize] {
                r[p] = Ordering::Greater;
                break;
            }
            if left[p][i as usize] < right[p][i as usize] {
                r[p] = Ordering::Less;
                break;
            }
            i -= 1;
        }
    }
    r
}
