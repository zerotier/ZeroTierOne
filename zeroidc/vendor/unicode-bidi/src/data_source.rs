// Copyright 2015 The Servo Project Developers. See the
// COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::BidiClass;

/// This trait abstracts over a data source that is able to produce the Unicode Bidi class for a given
/// character
pub trait BidiDataSource {
    fn bidi_class(&self, c: char) -> BidiClass;
}
