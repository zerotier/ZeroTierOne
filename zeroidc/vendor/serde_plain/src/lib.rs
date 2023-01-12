//! This crate implements a plain text serializer and deserializer.  It
//! can only serialize and deserialize primitives and derivatives thereof
//! (like basic enums or newtypes). It internally uses the
//! [`FromStr`](std::str::FromStr) and [`Display`](std::fmt::Display) trait to
//! convert objects around.
//!
//! The idea of this crate is that you can use the serde system to implement
//! [`FromStr`](std::str::FromStr) or [`Display`](std::fmt::Display) for your own
//! types based on the how serde would handle the type.
//!
//! # From String
//!
//! To parse a value from a string the [`from_str`] helper can be used:
//!
//! ```rust
//! assert_eq!(serde_plain::from_str::<i32>("42").unwrap(), 42);
//! ```
//!
//! This is particularly useful if enums are in use:
//!
//! ```rust
//! # #[macro_use] extern crate serde_derive;
//! use serde::Deserialize;
//!
//! # fn main() {
//! #[derive(Deserialize, Debug, PartialEq, Eq)]
//! pub enum MyEnum {
//!     VariantA,
//!     VariantB,
//! }
//!
//! assert_eq!(serde_plain::from_str::<MyEnum>("VariantA").unwrap(), MyEnum::VariantA);
//! # }
//! ```
//!
//! # To String
//!
//! The inverse is also possible with [`to_string`]:
//!
//! ```rust
//! assert_eq!(serde_plain::to_string(&true).unwrap(), "true");
//! ```
mod de;
mod error;
mod macros;
mod ser;

pub use crate::de::*;
pub use crate::error::*;
pub use crate::ser::*;
