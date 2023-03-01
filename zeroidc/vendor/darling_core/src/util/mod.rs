//! Utility types for attribute parsing.

use std::ops::{BitAnd, BitOr, Deref, Not};

use crate::{FromMeta, Result};

mod ident_string;
mod ignored;
mod over_ride;
mod parse_attribute;
mod path_list;
mod path_to_string;
mod spanned_value;
mod with_original;

pub use self::ident_string::IdentString;
pub use self::ignored::Ignored;
pub use self::over_ride::Override;
pub use self::parse_attribute::parse_attribute_to_meta_list;
pub use self::path_list::PathList;
pub use self::path_to_string::path_to_string;
pub use self::spanned_value::SpannedValue;
pub use self::with_original::WithOriginal;

/// Marker type equivalent to `Option<()>` for use in attribute parsing.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Flag(Option<()>);

impl Flag {
    /// Creates a new `Flag` which corresponds to the presence of a value.
    pub fn present() -> Self {
        Flag(Some(()))
    }
}

impl Deref for Flag {
    type Target = Option<()>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl FromMeta for Flag {
    fn from_meta(mi: &syn::Meta) -> Result<Self> {
        FromMeta::from_meta(mi).map(Flag)
    }
}

impl From<Flag> for bool {
    fn from(flag: Flag) -> Self {
        flag.is_some()
    }
}

impl From<bool> for Flag {
    fn from(v: bool) -> Self {
        if v {
            Flag::present()
        } else {
            Flag(None)
        }
    }
}

impl From<Option<()>> for Flag {
    fn from(v: Option<()>) -> Self {
        Flag::from(v.is_some())
    }
}

impl PartialEq<Option<()>> for Flag {
    fn eq(&self, rhs: &Option<()>) -> bool {
        self.0 == *rhs
    }
}

impl PartialEq<Flag> for Option<()> {
    fn eq(&self, rhs: &Flag) -> bool {
        *self == rhs.0
    }
}

impl PartialEq<bool> for Flag {
    fn eq(&self, rhs: &bool) -> bool {
        self.is_some() == *rhs
    }
}

impl Not for Flag {
    type Output = Self;

    fn not(self) -> Self {
        if self.is_some() {
            Flag(None)
        } else {
            Flag::present()
        }
    }
}

impl BitAnd for Flag {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self {
        #[allow(clippy::suspicious_arithmetic_impl)]
        (self.into() && rhs.into()).into()
    }
}

impl BitOr for Flag {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self {
        #[allow(clippy::suspicious_arithmetic_impl)]
        (self.into() || rhs.into()).into()
    }
}
