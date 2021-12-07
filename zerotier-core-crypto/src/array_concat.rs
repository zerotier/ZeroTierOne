/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::MaybeUninit;

#[inline(always)]
pub fn concat_2_slices<const S0: usize, const S1: usize, const S: usize>(s0: &[u8], s1: &[u8]) -> [u8; S] {
    debug_assert_eq!(S0 + S1, S);
    let mut tmp: [u8; S] = unsafe { MaybeUninit::uninit().assume_init() };
    tmp[..S0].copy_from_slice(s0);
    tmp[S0..].copy_from_slice(s1);
    tmp
}

#[inline(always)]
pub fn concat_2_arrays<const S0: usize, const S1: usize, const S: usize>(s0: &[u8; S0], s1: &[u8; S1]) -> [u8; S] {
    concat_2_slices::<S0, S1, S>(s0, s1)
}

#[inline(always)]
pub fn concat_3_slices<const S0: usize, const S1: usize, const S2: usize, const S: usize>(s0: &[u8], s1: &[u8], s2: &[u8]) -> [u8; S] {
    debug_assert_eq!(S0 + S1 + S2, S);
    let mut tmp: [u8; S] = unsafe { MaybeUninit::uninit().assume_init() };
    tmp[..S0].copy_from_slice(s0);
    tmp[S0..S1].copy_from_slice(s1);
    tmp[(S0 + S1)..].copy_from_slice(s2);
    tmp
}

#[inline(always)]
pub fn concat_3_arrays<const S0: usize, const S1: usize, const S2: usize, const S: usize>(s0: &[u8; S0], s1: &[u8; S1], s2: &[u8; S2]) -> [u8; S] {
    concat_3_slices::<S0, S1, S2, S>(s0, s1, s2)
}

#[inline(always)]
pub fn concat_4_slices<const S0: usize, const S1: usize, const S2: usize, const S3: usize, const S: usize>(s0: &[u8], s1: &[u8], s2: &[u8], s3: &[u8]) -> [u8; S] {
    debug_assert_eq!(S0 + S1 + S2 + S3, S);
    let mut tmp: [u8; S] = unsafe { MaybeUninit::uninit().assume_init() };
    tmp[..S0].copy_from_slice(s0);
    tmp[S0..S1].copy_from_slice(s1);
    tmp[(S0 + S1)..(S0 + S1 + S2)].copy_from_slice(s2);
    tmp[(S0 + S1 + S2)..].copy_from_slice(s3);
    tmp
}

#[inline(always)]
pub fn concat_4_arrays<const S0: usize, const S1: usize, const S2: usize, const S3: usize, const S: usize>(s0: &[u8; S0], s1: &[u8; S1], s2: &[u8; S2], s3: &[u8; S3]) -> [u8; S] {
    concat_4_slices::<S0, S1, S2, S3, S>(s0, s1, s2, s3)
}
