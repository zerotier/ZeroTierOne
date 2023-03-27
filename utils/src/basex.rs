use super::arrayvec::ArrayVec;

fn big_div_rem<const C: usize>(n: &mut ArrayVec<u32, C>, d: u32) -> u32 {
    while let Some(&0) = n.last() {
        n.pop();
    }
    let d = d as u64;
    let mut rem = 0;
    for word in n.iter_mut().rev() {
        let temp = (rem as u64).wrapping_shl(32) | (*word as u64);
        let (a, b) = (temp / d, temp % d);
        *word = a as u32;
        rem = b as u32;
    }
    while let Some(&0) = n.last() {
        n.pop();
    }
    rem
}

fn big_add<const C: usize>(n: &mut ArrayVec<u32, C>, i: u32) {
    debug_assert!(i <= (u32::MAX - 1));
    debug_assert!(!n.is_empty());
    debug_assert!(n.iter().any(|x| *x != 0));
    let mut carry = false;
    for word in n.iter_mut() {
        (*word, carry) = word.overflowing_add(i.wrapping_add(carry as u32));
    }
    if carry {
        n.push(1);
    }
}

fn big_mul<const C: usize>(n: &mut ArrayVec<u32, C>, m: u32) {
    while let Some(&0) = n.last() {
        n.pop();
    }
    let m = m as u64;
    let mut carry = 0;
    for word in n.iter_mut() {
        let temp = (*word as u64).wrapping_mul(m).wrapping_add(carry);
        *word = (temp & 0xffffffff) as u32;
        carry = temp.wrapping_shr(32);
    }
    if carry > 0 {
        n.push(carry as u32);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn div_rem() {
        let mut n = ArrayVec::<u32, 4>::new();
        n.push_slice(&[0xdeadbeef, 0xfeedfeed, 0xcafebabe, 0xf00dd00d]);
        let rem = big_div_rem(&mut n, 63);
        let nn = n.as_ref();
        assert!(nn[0] == 0xaa23440b && nn[1] == 0xa696103c && nn[2] == 0x89513fea && nn[3] == 0x03cf7514 && rem == 58);
    }
}
