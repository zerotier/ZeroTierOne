/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::any::TypeId;
use std::mem::size_of;

#[inline(always)]
pub fn same_type<U: 'static, V: 'static>() -> bool {
    TypeId::of::<U>() == TypeId::of::<V>() && size_of::<U>() == size_of::<V>()
}

#[inline(always)]
pub fn cast_ref<U: 'static, V: 'static>(u: &U) -> Option<&V> {
    if same_type::<U, V>() {
        Some(unsafe { std::mem::transmute::<&U, &V>(u) })
    } else {
        None
    }
}

#[inline(always)]
pub fn cast_mut<U: 'static, V: 'static>(u: &mut U) -> Option<&mut V> {
    if same_type::<U, V>() {
        Some(unsafe { std::mem::transmute::<&mut U, &mut V>(u) })
    } else {
        None
    }
}
