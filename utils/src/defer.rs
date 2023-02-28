/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

struct Defer<F: FnOnce()>(Option<F>);

impl<F: FnOnce()> Drop for Defer<F> {
    fn drop(&mut self) {
        self.0.take().map(|f| f());
    }
}

/// Defer execution of a closure until the return value is dropped.
///
/// This mimics the defer statement in Go, allowing you to always do some cleanup at
/// the end of a function no matter where it exits.
pub fn defer<F: FnOnce()>(f: F) -> impl Drop {
    Defer(Some(f))
}
