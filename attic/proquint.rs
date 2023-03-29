// This is a trimmed down version of: https://github.com/christian-blades-cb/proquint-rs
// BSD license

const UINT2CONSONANT: [char; 16] = ['b', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'n', 'p', 'r', 's', 't', 'v', 'z'];
const UINT2VOWEL: &'static [char] = &['a', 'i', 'o', 'u'];
const MASK_FIRST4_U16: u16 = 0xF000;
const MASK_FIRST2_U16: u16 = 0xC000;

macro_rules! decons {
    ($res:ident, $bitcounter:ident, $x:expr) => {{
        $bitcounter += 4;
        $res = $res.wrapping_shl(4);
        $res += $x;
    }};
}

macro_rules! devowel {
    ($res:ident, $bitcounter:ident, $x:expr) => {{
        $bitcounter += 2;
        $res = $res.wrapping_shl(2);
        $res += $x;
    }};
}

macro_rules! cons_u16 {
    ($i:ident, $out:ident) => {
        let j: u16 = ($i & MASK_FIRST4_U16).wrapping_shr(12);
        $i = $i.wrapping_shl(4);
        $out.push(UINT2CONSONANT[j as usize]);
    };
}

macro_rules! vowel_u16 {
    ($i:ident, $out:ident) => {
        let j: u16 = ($i & MASK_FIRST2_U16).wrapping_shr(14);
        $i = $i.wrapping_shl(2);
        $out.push(UINT2VOWEL[j as usize]);
    };
}

pub fn u16_from_quint(quint: &str) -> Option<u16> {
    let mut bitcounter = 0usize;
    let mut res = 0u16;
    for c in quint.chars() {
        match c {
            'b' => decons!(res, bitcounter, 0u16),
            'd' => decons!(res, bitcounter, 1u16),
            'f' => decons!(res, bitcounter, 2u16),
            'g' => decons!(res, bitcounter, 3u16),
            'h' => decons!(res, bitcounter, 4u16),
            'j' => decons!(res, bitcounter, 5u16),
            'k' => decons!(res, bitcounter, 6u16),
            'l' => decons!(res, bitcounter, 7u16),
            'm' => decons!(res, bitcounter, 8u16),
            'n' => decons!(res, bitcounter, 9u16),
            'p' => decons!(res, bitcounter, 10u16),
            'r' => decons!(res, bitcounter, 11u16),
            's' => decons!(res, bitcounter, 12u16),
            't' => decons!(res, bitcounter, 13u16),
            'v' => decons!(res, bitcounter, 14u16),
            'z' => decons!(res, bitcounter, 15u16),
            'a' => devowel!(res, bitcounter, 0u16),
            'i' => devowel!(res, bitcounter, 1u16),
            'o' => devowel!(res, bitcounter, 2u16),
            'u' => devowel!(res, bitcounter, 3u16),
            _ => {}
        }
    }
    if bitcounter == 16 {
        Some(res)
    } else {
        None
    }
}

pub fn u16_to_quint(mut i: u16, out: &mut String) {
    cons_u16!(i, out);
    vowel_u16!(i, out);
    cons_u16!(i, out);
    vowel_u16!(i, out);
    out.push(UINT2CONSONANT[(i & MASK_FIRST4_U16).wrapping_shr(12) as usize]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn quint_u16() {
        let mut s = String::with_capacity(16);
        for i in u16::MIN..=u16::MAX {
            s.clear();
            u16_to_quint(i, &mut s);
            assert_eq!(s.len(), 5);
            assert_eq!(u16_from_quint(s.as_str()).unwrap(), i);
        }
    }
}
