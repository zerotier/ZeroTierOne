#![warn(
    clippy::semicolon_if_nothing_returned,
    missing_copy_implementations,
    // missing_crate_level_docs, not available in MSRV
    missing_debug_implementations,
    missing_docs,
    rust_2018_idioms,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    variant_size_differences
)]
#![doc(test(attr(forbid(unsafe_code))))]
#![doc(test(attr(deny(
    missing_copy_implementations,
    missing_debug_implementations,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
))))]
#![doc(test(attr(warn(rust_2018_idioms))))]
// Not needed for 2018 edition and conflicts with `rust_2018_idioms`
#![doc(test(no_crate_inject))]
#![doc(html_root_url = "https://docs.rs/serde_with/1.14.0")]
#![cfg_attr(docsrs, feature(doc_cfg))]
#![allow(
    // clippy is broken and shows wrong warnings
    // clippy on stable does not know yet about the lint name
    unknown_lints,
    // https://github.com/rust-lang/rust-clippy/issues/8560
    clippy::only_used_in_recursion,
    // https://github.com/rust-lang/rust-clippy/issues/8867
    clippy::derive_partial_eq_without_eq,
)]
#![no_std]

//! [![crates.io badge](https://img.shields.io/crates/v/serde_with.svg)](https://crates.io/crates/serde_with/)
//! [![Build Status](https://github.com/jonasbb/serde_with/workflows/Rust%20CI/badge.svg)](https://github.com/jonasbb/serde_with)
//! [![codecov](https://codecov.io/gh/jonasbb/serde_with/branch/master/graph/badge.svg)](https://codecov.io/gh/jonasbb/serde_with)
//! [![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/4322/badge)](https://bestpractices.coreinfrastructure.org/projects/4322)
//! [![Binder](https://img.shields.io/badge/Try%20on%20-binder-579ACA.svg?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFkAAABZCAMAAABi1XidAAAB8lBMVEX///9XmsrmZYH1olJXmsr1olJXmsrmZYH1olJXmsr1olJXmsrmZYH1olL1olJXmsr1olJXmsrmZYH1olL1olJXmsrmZYH1olJXmsr1olL1olJXmsrmZYH1olL1olJXmsrmZYH1olL1olL0nFf1olJXmsrmZYH1olJXmsq8dZb1olJXmsrmZYH1olJXmspXmspXmsr1olL1olJXmsrmZYH1olJXmsr1olL1olJXmsrmZYH1olL1olLeaIVXmsrmZYH1olL1olL1olJXmsrmZYH1olLna31Xmsr1olJXmsr1olJXmsrmZYH1olLqoVr1olJXmsr1olJXmsrmZYH1olL1olKkfaPobXvviGabgadXmsqThKuofKHmZ4Dobnr1olJXmsr1olJXmspXmsr1olJXmsrfZ4TuhWn1olL1olJXmsqBi7X1olJXmspZmslbmMhbmsdemsVfl8ZgmsNim8Jpk8F0m7R4m7F5nLB6jbh7jbiDirOEibOGnKaMhq+PnaCVg6qWg6qegKaff6WhnpKofKGtnomxeZy3noG6dZi+n3vCcpPDcpPGn3bLb4/Mb47UbIrVa4rYoGjdaIbeaIXhoWHmZYHobXvpcHjqdHXreHLroVrsfG/uhGnuh2bwj2Hxk17yl1vzmljzm1j0nlX1olL3AJXWAAAAbXRSTlMAEBAQHx8gICAuLjAwMDw9PUBAQEpQUFBXV1hgYGBkcHBwcXl8gICAgoiIkJCQlJicnJ2goKCmqK+wsLC4usDAwMjP0NDQ1NbW3Nzg4ODi5+3v8PDw8/T09PX29vb39/f5+fr7+/z8/Pz9/v7+zczCxgAABC5JREFUeAHN1ul3k0UUBvCb1CTVpmpaitAGSLSpSuKCLWpbTKNJFGlcSMAFF63iUmRccNG6gLbuxkXU66JAUef/9LSpmXnyLr3T5AO/rzl5zj137p136BISy44fKJXuGN/d19PUfYeO67Znqtf2KH33Id1psXoFdW30sPZ1sMvs2D060AHqws4FHeJojLZqnw53cmfvg+XR8mC0OEjuxrXEkX5ydeVJLVIlV0e10PXk5k7dYeHu7Cj1j+49uKg7uLU61tGLw1lq27ugQYlclHC4bgv7VQ+TAyj5Zc/UjsPvs1sd5cWryWObtvWT2EPa4rtnWW3JkpjggEpbOsPr7F7EyNewtpBIslA7p43HCsnwooXTEc3UmPmCNn5lrqTJxy6nRmcavGZVt/3Da2pD5NHvsOHJCrdc1G2r3DITpU7yic7w/7Rxnjc0kt5GC4djiv2Sz3Fb2iEZg41/ddsFDoyuYrIkmFehz0HR2thPgQqMyQYb2OtB0WxsZ3BeG3+wpRb1vzl2UYBog8FfGhttFKjtAclnZYrRo9ryG9uG/FZQU4AEg8ZE9LjGMzTmqKXPLnlWVnIlQQTvxJf8ip7VgjZjyVPrjw1te5otM7RmP7xm+sK2Gv9I8Gi++BRbEkR9EBw8zRUcKxwp73xkaLiqQb+kGduJTNHG72zcW9LoJgqQxpP3/Tj//c3yB0tqzaml05/+orHLksVO+95kX7/7qgJvnjlrfr2Ggsyx0eoy9uPzN5SPd86aXggOsEKW2Prz7du3VID3/tzs/sSRs2w7ovVHKtjrX2pd7ZMlTxAYfBAL9jiDwfLkq55Tm7ifhMlTGPyCAs7RFRhn47JnlcB9RM5T97ASuZXIcVNuUDIndpDbdsfrqsOppeXl5Y+XVKdjFCTh+zGaVuj0d9zy05PPK3QzBamxdwtTCrzyg/2Rvf2EstUjordGwa/kx9mSJLr8mLLtCW8HHGJc2R5hS219IiF6PnTusOqcMl57gm0Z8kanKMAQg0qSyuZfn7zItsbGyO9QlnxY0eCuD1XL2ys/MsrQhltE7Ug0uFOzufJFE2PxBo/YAx8XPPdDwWN0MrDRYIZF0mSMKCNHgaIVFoBbNoLJ7tEQDKxGF0kcLQimojCZopv0OkNOyWCCg9XMVAi7ARJzQdM2QUh0gmBozjc3Skg6dSBRqDGYSUOu66Zg+I2fNZs/M3/f/Grl/XnyF1Gw3VKCez0PN5IUfFLqvgUN4C0qNqYs5YhPL+aVZYDE4IpUk57oSFnJm4FyCqqOE0jhY2SMyLFoo56zyo6becOS5UVDdj7Vih0zp+tcMhwRpBeLyqtIjlJKAIZSbI8SGSF3k0pA3mR5tHuwPFoa7N7reoq2bqCsAk1HqCu5uvI1n6JuRXI+S1Mco54YmYTwcn6Aeic+kssXi8XpXC4V3t7/ADuTNKaQJdScAAAAAElFTkSuQmCC)](https://mybinder.org/v2/gist/jonasbb/18b9aece4c17f617b1c2b3946d29eeb0/HEAD?filepath=serde-with-demo.ipynb)
//!
//! ---
//!
//! This crate provides custom de/serialization helpers to use in combination with [serde's with-annotation][with-annotation] and with the improved [`serde_as`][as-annotation]-annotation.
//! Some common use cases are:
//!
//! * De/Serializing a type using the `Display` and `FromStr` traits, e.g., for `u8`, `url::Url`, or `mime::Mime`.
//!      Check [`DisplayFromStr`][] or [`serde_with::rust::display_fromstr`][display_fromstr] for details.
//! * Support for arrays larger than 32 elements or using const generics.
//!     With `serde_as` large arrays are supported, even if they are nested in other types.
//!     `[bool; 64]`, `Option<[u8; M]>`, and `Box<[[u8; 64]; N]>` are all supported, as [this examples shows](#large-and-const-generic-arrays).
//! * Skip serializing all empty `Option` types with [`#[skip_serializing_none]`][skip_serializing_none].
//! * Apply a prefix to each field name of a struct, without changing the de/serialize implementations of the struct using [`with_prefix!`][].
//! * Deserialize a comma separated list like `#hash,#tags,#are,#great` into a `Vec<String>`.
//!      Check the documentation for [`serde_with::rust::StringWithSeparator::<CommaSeparator>`][StringWithSeparator].
//!
//! ## Getting Help
//!
//! **Check out the [user guide][user guide] to find out more tips and tricks about this crate.**
//!
//! For further help using this crate you can [open a new discussion](https://github.com/jonasbb/serde_with/discussions/new) or ask on [users.rust-lang.org](https://users.rust-lang.org/).
//! For bugs, please open a [new issue](https://github.com/jonasbb/serde_with/issues/new) on GitHub.
//!
//! # Use `serde_with` in your Project
//!
//! Add this to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies.serde_with]
//! version = "1.14.0"
//! features = [ "..." ]
//! ```
//!
//! The crate contains different features for integration with other common crates.
//! Check the [feature flags][] section for information about all available features.
//!
//! # Examples
//!
//! Annotate your struct or enum to enable the custom de/serializer.
//! The `#[serde_as]` attribute must be place *before* the `#[derive]`.
//!
//! ## `DisplayFromStr`
//!
//! ```rust
//! # #[cfg(feature = "macros")]
//! # use serde::{Deserialize, Serialize};
//! # #[cfg(feature = "macros")]
//! # use serde_with::{serde_as, DisplayFromStr};
//! # #[cfg(feature = "macros")]
//! #[serde_as]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Foo {
//!     // Serialize with Display, deserialize with FromStr
//!     #[serde_as(as = "DisplayFromStr")]
//!     bar: u8,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json"))] {
//! // This will serialize
//! # let foo =
//! Foo {bar: 12}
//! # ;
//!
//! // into this JSON
//! # let json = r#"
//! {"bar": "12"}
//! # "#;
//! # assert_eq!(json.replace(" ", "").replace("\n", ""), serde_json::to_string(&foo).unwrap());
//! # assert_eq!(foo, serde_json::from_str(&json).unwrap());
//! # }
//! ```
//!
//! ## Large and const-generic arrays
//!
//! serde does not support arrays with more than 32 elements or using const-generics.
//! The `serde_as` attribute allows circumventing this restriction, even for nested types and nested arrays.
//!
//! ```rust
//! # #[cfg(FALSE)] {
//! # #[cfg(feature = "macros")]
//! # use serde::{Deserialize, Serialize};
//! # #[cfg(feature = "macros")]
//! # use serde_with::serde_as;
//! # #[cfg(feature = "macros")]
//! #[serde_as]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Arrays<const N: usize, const M: usize> {
//!     #[serde_as(as = "[_; N]")]
//!     constgeneric: [bool; N],
//!
//!     #[serde_as(as = "Box<[[_; 64]; N]>")]
//!     nested: Box<[[u8; 64]; N]>,
//!
//!     #[serde_as(as = "Option<[_; M]>")]
//!     optional: Option<[u8; M]>,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json"))] {
//! // This allows us to serialize a struct like this
//! let arrays: Arrays<100, 128> = Arrays {
//!     constgeneric: [true; 100],
//!     nested: Box::new([[111; 64]; 100]),
//!     optional: Some([222; 128])
//! };
//! assert!(serde_json::to_string(&arrays).is_ok());
//! # }
//! # }
//! ```
//!
//! ## `skip_serializing_none`
//!
//! This situation often occurs with JSON, but other formats also support optional fields.
//! If many fields are optional, putting the annotations on the structs can become tedious.
//! The `#[skip_serializing_none]` attribute must be place *before* the `#[derive]`.
//!
//! ```rust
//! # #[cfg(feature = "macros")]
//! # use serde::{Deserialize, Serialize};
//! # #[cfg(feature = "macros")]
//! # use serde_with::skip_serializing_none;
//! # #[cfg(feature = "macros")]
//! #[skip_serializing_none]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Foo {
//!     a: Option<usize>,
//!     b: Option<usize>,
//!     c: Option<usize>,
//!     d: Option<usize>,
//!     e: Option<usize>,
//!     f: Option<usize>,
//!     g: Option<usize>,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json"))] {
//! // This will serialize
//! # let foo =
//! Foo {a: None, b: None, c: None, d: Some(4), e: None, f: None, g: Some(7)}
//! # ;
//!
//! // into this JSON
//! # let json = r#"
//! {"d": 4, "g": 7}
//! # "#;
//! # assert_eq!(json.replace(" ", "").replace("\n", ""), serde_json::to_string(&foo).unwrap());
//! # assert_eq!(foo, serde_json::from_str(&json).unwrap());
//! # }
//! ```
//!
//! ## Advanced `serde_as` usage
//!
//! This example is mainly supposed to highlight the flexibility of the `serde_as`-annotation compared to [serde's with-annotation][with-annotation].
//! More details about `serde_as` can be found in the [user guide][].
//!
//! ```rust
//! # #[cfg(all(feature = "macros", feature = "hex"))]
//! # use {
//! #     serde::{Deserialize, Serialize},
//! #     serde_with::{serde_as, DisplayFromStr, DurationSeconds, hex::Hex},
//! #     std::time::Duration,
//! #     std::collections::BTreeMap,
//! # };
//! # #[cfg(all(feature = "macros", feature = "hex"))]
//! #[serde_as]
//! # #[derive(Debug, Eq, PartialEq)]
//! #[derive(Deserialize, Serialize)]
//! struct Foo {
//!      // Serialize them into a list of number as seconds
//!      #[serde_as(as = "Vec<DurationSeconds>")]
//!      durations: Vec<Duration>,
//!      // We can treat a Vec like a map with duplicates.
//!      // JSON only allows string keys, so convert i32 to strings
//!      // The bytes will be hex encoded
//!      #[serde_as(as = "BTreeMap<DisplayFromStr, Hex>")]
//!      bytes: Vec<(i32, Vec<u8>)>,
//! }
//!
//! # #[cfg(all(feature = "macros", feature = "json", feature = "hex"))] {
//! // This will serialize
//! # let foo =
//! Foo {
//!     durations: vec![Duration::new(5, 0), Duration::new(3600, 0), Duration::new(0, 0)],
//!     bytes: vec![
//!         (1, vec![0, 1, 2]),
//!         (-100, vec![100, 200, 255]),
//!         (1, vec![0, 111, 222]),
//!     ],
//! }
//! # ;
//!
//! // into this JSON
//! # let json = r#"
//! {
//!     "durations": [5, 3600, 0],
//!     "bytes": {
//!         "1": "000102",
//!         "-100": "64c8ff",
//!         "1": "006fde"
//!     }
//! }
//! # "#;
//! # assert_eq!(json.replace(" ", "").replace("\n", ""), serde_json::to_string(&foo).unwrap());
//! # assert_eq!(foo, serde_json::from_str(&json).unwrap());
//! # }
//! ```
//!
//! [`DisplayFromStr`]: https://docs.rs/serde_with/1.14.0/serde_with/struct.DisplayFromStr.html
//! [`with_prefix!`]: https://docs.rs/serde_with/1.14.0/serde_with/macro.with_prefix.html
//! [display_fromstr]: https://docs.rs/serde_with/1.14.0/serde_with/rust/display_fromstr/index.html
//! [feature flags]: https://docs.rs/serde_with/1.14.0/serde_with/guide/feature_flags/index.html
//! [skip_serializing_none]: https://docs.rs/serde_with/1.14.0/serde_with/attr.skip_serializing_none.html
//! [StringWithSeparator]: https://docs.rs/serde_with/1.14.0/serde_with/rust/struct.StringWithSeparator.html
//! [user guide]: https://docs.rs/serde_with/1.14.0/serde_with/guide/index.html
//! [with-annotation]: https://serde.rs/field-attrs.html#with
//! [as-annotation]: https://docs.rs/serde_with/1.14.0/serde_with/guide/serde_as/index.html

extern crate alloc;
#[doc(hidden)]
pub extern crate serde;
extern crate std;

#[cfg(feature = "base64")]
#[cfg_attr(docsrs, doc(cfg(feature = "base64")))]
pub mod base64;
#[cfg(feature = "chrono")]
#[cfg_attr(docsrs, doc(cfg(feature = "chrono")))]
pub mod chrono;
mod content;
pub mod de;
mod duplicate_key_impls;
mod enum_map;
mod flatten_maybe;
pub mod formats;
#[cfg(feature = "hex")]
#[cfg_attr(docsrs, doc(cfg(feature = "hex")))]
pub mod hex;
#[cfg(feature = "json")]
#[cfg_attr(docsrs, doc(cfg(feature = "json")))]
pub mod json;
pub mod rust;
pub mod ser;
mod serde_conv;
#[cfg(feature = "time_0_3")]
#[cfg_attr(docsrs, doc(cfg(feature = "time_0_3")))]
pub mod time_0_3;
mod utils;
#[doc(hidden)]
pub mod with_prefix;

// Taken from shepmaster/snafu
// Originally licensed as MIT+Apache 2
// https://github.com/shepmaster/snafu/blob/fd37d79d4531ed1d3eebffad0d658928eb860cfe/src/lib.rs#L121-L165
#[cfg(feature = "guide")]
#[allow(unused_macro_rules)]
macro_rules! generate_guide {
    (pub mod $name:ident; $($rest:tt)*) => {
        generate_guide!(@gen ".", pub mod $name { } $($rest)*);
    };
    (pub mod $name:ident { $($children:tt)* } $($rest:tt)*) => {
        generate_guide!(@gen ".", pub mod $name { $($children)* } $($rest)*);
    };
    (@gen $prefix:expr, ) => {};
    (@gen $prefix:expr, pub mod $name:ident; $($rest:tt)*) => {
        generate_guide!(@gen $prefix, pub mod $name { } $($rest)*);
    };
    (@gen $prefix:expr, @code pub mod $name:ident; $($rest:tt)*) => {
        pub mod $name;
        generate_guide!(@gen $prefix, $($rest)*);
    };
    (@gen $prefix:expr, pub mod $name:ident { $($children:tt)* } $($rest:tt)*) => {
        doc_comment::doc_comment! {
            include_str!(concat!($prefix, "/", stringify!($name), ".md")),
            pub mod $name {
                generate_guide!(@gen concat!($prefix, "/", stringify!($name)), $($children)*);
            }
        }
        generate_guide!(@gen $prefix, $($rest)*);
    };
}

#[cfg(feature = "guide")]
generate_guide! {
    pub mod guide {
        pub mod feature_flags;
        pub mod serde_as;
        pub mod serde_as_transformations;
    }
}

#[doc(inline)]
pub use crate::{
    de::DeserializeAs, enum_map::EnumMap, rust::StringWithSeparator, ser::SerializeAs,
};
use core::marker::PhantomData;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
// Re-Export all proc_macros, as these should be seen as part of the serde_with crate
#[cfg(feature = "macros")]
#[cfg_attr(docsrs, doc(cfg(feature = "macros")))]
#[doc(inline)]
pub use serde_with_macros::*;

/// Separator for string-based collection de/serialization
pub trait Separator {
    /// Return the string delimiting two elements in the string-based collection
    fn separator() -> &'static str;
}

/// Predefined separator using a single space
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default)]
pub struct SpaceSeparator;

impl Separator for SpaceSeparator {
    #[inline]
    fn separator() -> &'static str {
        " "
    }
}

/// Predefined separator using a single comma
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default)]
pub struct CommaSeparator;

impl Separator for CommaSeparator {
    #[inline]
    fn separator() -> &'static str {
        ","
    }
}

/// Adapter to convert from `serde_as` to the serde traits.
///
/// The `As` type adapter allows using types which implement [`DeserializeAs`] or [`SerializeAs`] in place of serde's with-annotation.
/// The with-annotation allows running custom code when de/serializing, however it is quite inflexible.
/// The traits [`DeserializeAs`]/[`SerializeAs`] are more flexible, as they allow composition and nesting of types to create more complex de/serialization behavior.
/// However, they are not directly compatible with serde, as they are not provided by serde.
/// The `As` type adapter makes them compatible, by forwarding the function calls to `serialize`/`deserialize` to the corresponding functions `serialize_as` and `deserialize_as`.
///
/// It is not required to use this type directly.
/// Instead, it is highly encouraged to use the [`#[serde_as]`][serde_as] attribute since it includes further usability improvements.
/// If the use of the use of the proc-macro is not acceptable, then `As` can be used directly with serde.
///
/// ```rust
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::{As, DisplayFromStr};
/// #
/// #[derive(Deserialize, Serialize)]
/// # struct S {
/// // Serialize numbers as sequence of strings, using Display and FromStr
/// #[serde(with = "As::<Vec<DisplayFromStr>>")]
/// field: Vec<u8>,
/// # }
/// ```
/// If the normal `Deserialize`/`Serialize` traits should be used, the placeholder type [`Same`] can be used.
/// It implements [`DeserializeAs`][]/[`SerializeAs`][], when the underlying type implements `Deserialize`/`Serialize`.
///
/// ```rust
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::{As, DisplayFromStr, Same};
/// # use std::collections::BTreeMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// # struct S {
/// // Serialize map, turn keys into strings but keep type of value
/// #[serde(with = "As::<BTreeMap<DisplayFromStr, Same>>")]
/// field: BTreeMap<u8, i32>,
/// # }
/// ```
///
/// [serde_as]: https://docs.rs/serde_with/1.14.0/serde_with/attr.serde_as.html
#[derive(Copy, Clone, Debug, Default)]
pub struct As<T: ?Sized>(PhantomData<T>);

impl<T: ?Sized> As<T> {
    /// Serialize type `T` using [`SerializeAs`][]
    ///
    /// The function signature is compatible with [serde's with-annotation][with-annotation].
    ///
    /// [with-annotation]: https://serde.rs/field-attrs.html#with
    pub fn serialize<S, I>(value: &I, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: SerializeAs<I>,
        I: ?Sized,
    {
        T::serialize_as(value, serializer)
    }

    /// Deserialize type `T` using [`DeserializeAs`][]
    ///
    /// The function signature is compatible with [serde's with-annotation][with-annotation].
    ///
    /// [with-annotation]: https://serde.rs/field-attrs.html#with
    pub fn deserialize<'de, D, I>(deserializer: D) -> Result<I, D::Error>
    where
        T: DeserializeAs<'de, I>,
        D: Deserializer<'de>,
    {
        T::deserialize_as(deserializer)
    }
}

/// Adapter to convert from `serde_as` to the serde traits.
///
/// This is the counter-type to [`As`][].
/// It can be used whenever a type implementing [`DeserializeAs`][]/[`SerializeAs`][] is required but the normal `Deserialize`/`Serialize` traits should be used.
/// Check [`As`] for an example.
#[derive(Copy, Clone, Debug, Default)]
pub struct Same;

/// De/Serialize using [`Display`] and [`FromStr`] implementation
///
/// This allows deserializing a string as a number.
/// It can be very useful for serialization formats like JSON, which do not support integer
/// numbers and have to resort to strings to represent them.
///
/// Another use case is types with [`Display`] and [`FromStr`] implementations, but without serde
/// support, which can be found in some crates.
///
/// If you control the type you want to de/serialize, you can instead use the two derive macros, [`SerializeDisplay`] and [`DeserializeFromStr`].
/// They properly implement the traits [`Serialize`] and [`Deserialize`] such that user of the type no longer have to use the `serde_as` system.
///
/// The same functionality is also available as [`serde_with::rust::display_fromstr`][crate::rust::display_fromstr] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "DisplayFromStr")]
///     mime: mime::Mime,
///     #[serde_as(as = "DisplayFromStr")]
///     number: u32,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "mime": "text/plain",
///     "number": "159",
/// })).unwrap();
/// assert_eq!(mime::TEXT_PLAIN, v.mime);
/// assert_eq!(159, v.number);
///
/// let x = A {
///     mime: mime::STAR_STAR,
///     number: 777,
/// };
/// assert_eq!(json!({ "mime": "*/*", "number": "777" }), serde_json::to_value(&x).unwrap());
/// # }
/// ```
///
/// [`Display`]: std::fmt::Display
/// [`FromStr`]: std::str::FromStr
#[derive(Copy, Clone, Debug, Default)]
pub struct DisplayFromStr;

/// De/Serialize a [`Option<String>`] type while transforming the empty string to [`None`]
///
/// Convert an [`Option<T>`] from/to string using [`FromStr`] and [`AsRef<str>`] implementations.
/// An empty string is deserialized as [`None`] and a [`None`] vice versa.
///
/// The same functionality is also available as [`serde_with::rust::string_empty_as_none`][crate::rust::string_empty_as_none] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, NoneAsEmptyString};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "NoneAsEmptyString")]
///     tags: Option<String>,
/// }
///
/// let v: A = serde_json::from_value(json!({ "tags": "" })).unwrap();
/// assert_eq!(None, v.tags);
///
/// let v: A = serde_json::from_value(json!({ "tags": "Hi" })).unwrap();
/// assert_eq!(Some("Hi".to_string()), v.tags);
///
/// let x = A {
///     tags: Some("This is text".to_string()),
/// };
/// assert_eq!(json!({ "tags": "This is text" }), serde_json::to_value(&x).unwrap());
///
/// let x = A {
///     tags: None,
/// };
/// assert_eq!(json!({ "tags": "" }), serde_json::to_value(&x).unwrap());
/// # }
/// ```
///
/// [`FromStr`]: std::str::FromStr
#[derive(Copy, Clone, Debug, Default)]
pub struct NoneAsEmptyString;

/// Deserialize value and return [`Default`] on error
///
/// The main use case is ignoring error while deserializing.
/// Instead of erroring, it simply deserializes the [`Default`] variant of the type.
/// It is not possible to find the error location, i.e., which field had a deserialization error, with this method.
/// During serialization this wrapper does nothing.
/// The serialization behavior of the underlying type is preserved.
/// The type must implement [`Default`] for this conversion to work.
///
/// The same functionality is also available as [`serde_with::rust::default_on_error`][crate::rust::default_on_error] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, DefaultOnError};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Debug)]
/// struct A {
///     #[serde_as(deserialize_as = "DefaultOnError")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// // null is of invalid type
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // String is of invalid type
/// let a: A = serde_json::from_str(r#"{"value": "123"}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // Map is of invalid type
/// let a: A = dbg!(serde_json::from_str(r#"{"value": {}}"#)).unwrap();
/// assert_eq!(0, a.value);
///
/// // Missing entries still cause errors
/// assert!(serde_json::from_str::<A>(r#"{  }"#).is_err());
/// # }
/// ```
///
/// Deserializing missing values can be supported by adding the `default` field attribute:
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, DefaultOnError};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct B {
///     #[serde_as(deserialize_as = "DefaultOnError")]
///     #[serde(default)]
///     value: u32,
/// }
///
/// let b: B = serde_json::from_str(r#"{  }"#).unwrap();
/// assert_eq!(0, b.value);
/// # }
/// ```
///
/// `DefaultOnError` can be combined with other conversion methods.
/// In this example, we deserialize a `Vec`, each element is deserialized from a string.
/// If the string does not parse as a number, then we get the default value of 0.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DefaultOnError, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Serialize, Deserialize)]
/// struct C {
///     #[serde_as(as = "Vec<DefaultOnError<DisplayFromStr>>")]
///     value: Vec<u32>,
/// };
///
/// let c: C = serde_json::from_value(json!({
///     "value": ["1", "2", "a3", "", {}, "6"]
/// })).unwrap();
/// assert_eq!(vec![1, 2, 0, 0, 0, 6], c.value);
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct DefaultOnError<T = Same>(PhantomData<T>);

/// Deserialize [`Default`] from `null` values
///
/// Instead of erroring on `null` values, it simply deserializes the [`Default`] variant of the type.
/// During serialization this wrapper does nothing.
/// The serialization behavior of the underlying type is preserved.
/// The type must implement [`Default`] for this conversion to work.
///
/// The same functionality is also available as [`serde_with::rust::default_on_null`][crate::rust::default_on_null] compatible with serde's with-annotation.
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, DefaultOnNull};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Debug)]
/// struct A {
///     #[serde_as(deserialize_as = "DefaultOnNull")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// // null values are deserialized into the default, here 0
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
/// # }
/// ```
///
/// `DefaultOnNull` can be combined with other conversion methods.
/// In this example, we deserialize a `Vec`, each element is deserialized from a string.
/// If we encounter null, then we get the default value of 0.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DefaultOnNull, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Serialize, Deserialize)]
/// struct C {
///     #[serde_as(as = "Vec<DefaultOnNull<DisplayFromStr>>")]
///     value: Vec<u32>,
/// };
///
/// let c: C = serde_json::from_value(json!({
///     "value": ["1", "2", null, null, "5"]
/// })).unwrap();
/// assert_eq!(vec![1, 2, 0, 0, 5], c.value);
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct DefaultOnNull<T = Same>(PhantomData<T>);

/// Deserialize from bytes or string
///
/// Any Rust [`String`] can be converted into bytes, i.e., `Vec<u8>`.
/// Accepting both as formats while deserializing can be helpful while interacting with language
/// which have a looser definition of string than Rust.
///
/// # Example
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, BytesOrString};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "BytesOrString")]
///     bytes_or_string: Vec<u8>,
/// }
///
/// // Here we deserialize from a byte array ...
/// let j = json!({
///   "bytes_or_string": [
///     0,
///     1,
///     2,
///     3
///   ]
/// });
///
/// let a: A = serde_json::from_value(j.clone()).unwrap();
/// assert_eq!(vec![0, 1, 2, 3], a.bytes_or_string);
///
/// // and serialization works too.
/// assert_eq!(j, serde_json::to_value(&a).unwrap());
///
/// // But we also support deserializing from a String
/// let j = json!({
///   "bytes_or_string": "✨Works!"
/// });
///
/// let a: A = serde_json::from_value(j).unwrap();
/// assert_eq!("✨Works!".as_bytes(), &*a.bytes_or_string);
/// # }
/// ```
/// [`String`]: std::string::String
#[derive(Copy, Clone, Debug, Default)]
pub struct BytesOrString;

/// De/Serialize Durations as number of seconds.
///
/// De/serialize durations as number of seconds with subsecond precision.
/// Subsecond precision is *only* supported for [`DurationSecondsWithFrac`], but not for [`DurationSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `u64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct duration and allows deserialization from any type.
/// For example, deserializing `DurationSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::Duration`] with the `chrono`-[feature flag].
/// This type also supports [`time::Duration`][::time_0_3::Duration] with the `time_0_3`-[feature flag].
///
/// | Duration Type         | Converter                 | Available `FORMAT`s    |
/// | --------------------- | ------------------------- | ---------------------- |
/// | `std::time::Duration` | `DurationSeconds`         | `u64`, `f64`, `String` |
/// | `std::time::Duration` | `DurationSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::Duration`    | `DurationSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::Duration`    | `DurationSecondsWithFrac` | `f64`, `String`        |
/// | `time::Duration`      | `DurationSeconds`         | `i64`, `f64`, `String` |
/// | `time::Duration`      | `DurationSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSeconds};
/// use std::time::Duration;
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSeconds<u64>")]
///     d_u64: Duration,
///     #[serde_as(as = "DurationSeconds<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSeconds<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let d = Durations {
///     d_u64: Duration::new(12345, 0), // Create from seconds and nanoseconds
///     d_f64: Duration::new(12345, 500_000_000),
///     d_string: Duration::new(12345, 999_999_999),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "d_u64": 12345,
///     "d_f64": 12346.0,
///     "d_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "d_u64": 12345,
///     "d_f64": 12345.5,
///     "d_string": "12346",
/// });
/// let expected = Durations {
///     d_u64: Duration::new(12345, 0), // Create from seconds and nanoseconds
///     d_f64: Duration::new(12346, 0),
///     d_string: Duration::new(12346, 0),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`] is also supported when using the `chrono` feature.
/// It is a signed duration, thus can be de/serialized as an `i64` instead of a `u64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSeconds};
/// # use chrono_crate::Duration;
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::Duration;
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSeconds<i64>")]
///     d_i64: Duration,
///     #[serde_as(as = "DurationSeconds<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSeconds<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let d = Durations {
///     d_i64: Duration::seconds(-12345),
///     d_f64: Duration::seconds(-12345) + Duration::milliseconds(500),
///     d_string: Duration::seconds(12345) + Duration::nanoseconds(999_999_999),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "d_i64": -12345,
///     "d_f64": -12345.0,
///     "d_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "d_i64": -12345,
///     "d_f64": -12345.5,
///     "d_string": "12346",
/// });
/// let expected = Durations {
///     d_i64: Duration::seconds(-12345),
///     d_f64: Duration::seconds(-12346),
///     d_string: Duration::seconds(12346),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`]: chrono_crate::Duration
/// [feature flag]: https://docs.rs/serde_with/1.14.0/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// De/Serialize Durations as number of seconds.
///
/// De/serialize durations as number of seconds with subsecond precision.
/// Subsecond precision is *only* supported for [`DurationSecondsWithFrac`], but not for [`DurationSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `u64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct duration and allows deserialization from any type.
/// For example, deserializing `DurationSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::Duration`] with the `chrono`-[feature flag].
/// This type also supports [`time::Duration`][::time_0_3::Duration] with the `time_0_3`-[feature flag].
///
/// | Duration Type         | Converter                 | Available `FORMAT`s    |
/// | --------------------- | ------------------------- | ---------------------- |
/// | `std::time::Duration` | `DurationSeconds`         | `u64`, `f64`, `String` |
/// | `std::time::Duration` | `DurationSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::Duration`    | `DurationSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::Duration`    | `DurationSecondsWithFrac` | `f64`, `String`        |
/// | `time::Duration`      | `DurationSeconds`         | `i64`, `f64`, `String` |
/// | `time::Duration`      | `DurationSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSecondsWithFrac};
/// use std::time::Duration;
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSecondsWithFrac<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSecondsWithFrac<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let d = Durations {
///     d_f64: Duration::new(12345, 500_000_000), // Create from seconds and nanoseconds
///     d_string: Duration::new(12345, 999_999_000),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "d_f64": 12345.5,
///     "d_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "d_f64": 12345.5,
///     "d_string": "12345.987654",
/// });
/// let expected = Durations {
///     d_f64: Duration::new(12345, 500_000_000), // Create from seconds and nanoseconds
///     d_string: Duration::new(12345, 987_654_000),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`] is also supported when using the `chrono` feature.
/// It is a signed duration, thus can be de/serialized as an `i64` instead of a `u64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DurationSecondsWithFrac};
/// # use chrono_crate::Duration;
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::Duration;
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Durations {
///     #[serde_as(as = "DurationSecondsWithFrac<f64>")]
///     d_f64: Duration,
///     #[serde_as(as = "DurationSecondsWithFrac<String>")]
///     d_string: Duration,
/// };
///
/// // Serialization
///
/// let d = Durations {
///     d_f64: Duration::seconds(-12345) + Duration::milliseconds(500),
///     d_string: Duration::seconds(12345) + Duration::nanoseconds(999_999_000),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "d_f64": -12344.5,
///     "d_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&d).unwrap());
///
/// // Deserialization works too
///
/// let json = json!({
///     "d_f64": -12344.5,
///     "d_string": "12345.987",
/// });
/// let expected = Durations {
///     d_f64: Duration::seconds(-12345) + Duration::milliseconds(500),
///     d_string: Duration::seconds(12345) + Duration::milliseconds(987),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::Duration`]: chrono_crate::Duration
/// [feature flag]: https://docs.rs/serde_with/1.14.0/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSeconds`] with milli-seconds as base unit.
///
/// This type is equivalent to [`DurationSeconds`] except that each unit represents 1 milli-second instead of 1 second for [`DurationSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMilliSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSecondsWithFrac`] with milli-seconds as base unit.
///
/// This type is equivalent to [`DurationSecondsWithFrac`] except that each unit represents 1 milli-second instead of 1 second for [`DurationSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMilliSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSeconds`] with micro-seconds as base unit.
///
/// This type is equivalent to [`DurationSeconds`] except that each unit represents 1 micro-second instead of 1 second for [`DurationSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMicroSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSecondsWithFrac`] with micro-seconds as base unit.
///
/// This type is equivalent to [`DurationSecondsWithFrac`] except that each unit represents 1 micro-second instead of 1 second for [`DurationSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationMicroSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSeconds`] with nano-seconds as base unit.
///
/// This type is equivalent to [`DurationSeconds`] except that each unit represents 1 nano-second instead of 1 second for [`DurationSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationNanoSeconds<
    FORMAT: formats::Format = u64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`DurationSecondsWithFrac`] with nano-seconds as base unit.
///
/// This type is equivalent to [`DurationSecondsWithFrac`] except that each unit represents 1 nano-second instead of 1 second for [`DurationSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct DurationNanoSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// De/Serialize timestamps as seconds since the UNIX epoch
///
/// De/serialize timestamps as seconds since the UNIX epoch.
/// Subsecond precision is *only* supported for [`TimestampSecondsWithFrac`], but not for [`TimestampSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `i64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct timestamp and allows deserialization from any type.
/// For example, deserializing `TimestampSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::DateTime`][DateTime] with the `chrono`-[feature flag].
/// This type also supports [`time::OffsetDateTime`][::time_0_3::OffsetDateTime] and [`time::PrimitiveDateTime`][::time_0_3::PrimitiveDateTime] with the `time_0_3`-[feature flag].
///
/// | Timestamp Type            | Converter                  | Available `FORMAT`s    |
/// | ------------------------- | -------------------------- | ---------------------- |
/// | `std::time::SystemTime`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `std::time::SystemTime`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Utc>`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Utc>`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Local>` | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Local>` | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `time::OffsetDateTime`    | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `time::OffsetDateTime`    | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `time::PrimitiveDateTime` | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `time::PrimitiveDateTime` | `TimestampSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSeconds};
/// use std::time::{Duration, SystemTime};
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSeconds<i64>")]
///     st_i64: SystemTime,
///     #[serde_as(as = "TimestampSeconds<f64>")]
///     st_f64: SystemTime,
///     #[serde_as(as = "TimestampSeconds<String>")]
///     st_string: SystemTime,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let ts = Timestamps {
///     st_i64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 0)).unwrap(),
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 500_000_000)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 999_999_999)).unwrap(),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "st_i64": 12345,
///     "st_f64": 12346.0,
///     "st_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "st_i64": 12345,
///     "st_f64": 12345.5,
///     "st_string": "12346",
/// });
/// let expected  = Timestamps {
///     st_i64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 0)).unwrap(),
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12346, 0)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12346, 0)).unwrap(),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::DateTime<Utc>`][DateTime] and [`chrono::DateTime<Local>`][DateTime] are also supported when using the `chrono` feature.
/// Like [`SystemTime`], it is a signed timestamp, thus can be de/serialized as an `i64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSeconds};
/// # use chrono_crate::{DateTime, Local, TimeZone, Utc};
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::{DateTime, Local, TimeZone, Utc};
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSeconds<i64>")]
///     dt_i64: DateTime<Utc>,
///     #[serde_as(as = "TimestampSeconds<f64>")]
///     dt_f64: DateTime<Local>,
///     #[serde_as(as = "TimestampSeconds<String>")]
///     dt_string: DateTime<Utc>,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let ts = Timestamps {
///     dt_i64: Utc.timestamp(-12345, 0),
///     dt_f64: Local.timestamp(-12345, 500_000_000),
///     dt_string: Utc.timestamp(12345, 999_999_999),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "dt_i64": -12345,
///     "dt_f64": -12345.0,
///     "dt_string": "12346",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "dt_i64": -12345,
///     "dt_f64": -12345.5,
///     "dt_string": "12346",
/// });
/// let expected = Timestamps {
///     dt_i64: Utc.timestamp(-12345, 0),
///     dt_f64: Local.timestamp(-12346, 0),
///     dt_string: Utc.timestamp(12346, 0),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`SystemTime`]: std::time::SystemTime
/// [DateTime]: chrono_crate::DateTime
/// [feature flag]: https://docs.rs/serde_with/1.14.0/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// De/Serialize timestamps as seconds since the UNIX epoch
///
/// De/serialize timestamps as seconds since the UNIX epoch.
/// Subsecond precision is *only* supported for [`TimestampSecondsWithFrac`], but not for [`TimestampSeconds`].
/// You can configure the serialization format between integers, floats, and stringified numbers with the `FORMAT` specifier and configure the deserialization with the `STRICTNESS` specifier.
///
/// The `STRICTNESS` specifier can either be [`formats::Strict`] or [`formats::Flexible`] and defaults to [`formats::Strict`].
/// [`formats::Strict`] means that deserialization only supports the type given in `FORMAT`, e.g., if `FORMAT` is `i64` deserialization from a `f64` will error.
/// [`formats::Flexible`] means that deserialization will perform a best effort to extract the correct timestamp and allows deserialization from any type.
/// For example, deserializing `TimestampSeconds<f64, Flexible>` will discard any subsecond precision during deserialization from `f64` and will parse a `String` as an integer number.
///
/// This type also supports [`chrono::DateTime`][DateTime] and [`chrono::NaiveDateTime`][NaiveDateTime] with the `chrono`-[feature flag].
/// This type also supports [`time::OffsetDateTime`][::time_0_3::OffsetDateTime] and [`time::PrimitiveDateTime`][::time_0_3::PrimitiveDateTime] with the `time_0_3`-[feature flag].
///
/// | Timestamp Type            | Converter                  | Available `FORMAT`s    |
/// | ------------------------- | -------------------------- | ---------------------- |
/// | `std::time::SystemTime`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `std::time::SystemTime`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Utc>`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Utc>`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::DateTime<Local>` | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::DateTime<Local>` | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `chrono::NaiveDateTime`   | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `chrono::NaiveDateTime`   | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `time::OffsetDateTime`    | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `time::OffsetDateTime`    | `TimestampSecondsWithFrac` | `f64`, `String`        |
/// | `time::PrimitiveDateTime` | `TimestampSeconds`         | `i64`, `f64`, `String` |
/// | `time::PrimitiveDateTime` | `TimestampSecondsWithFrac` | `f64`, `String`        |
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSecondsWithFrac};
/// use std::time::{Duration, SystemTime};
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSecondsWithFrac<f64>")]
///     st_f64: SystemTime,
///     #[serde_as(as = "TimestampSecondsWithFrac<String>")]
///     st_string: SystemTime,
/// };
///
/// // Serialization
/// // See how the values get rounded, since subsecond precision is not allowed.
///
/// let ts = Timestamps {
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 500_000_000)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 999_999_000)).unwrap(),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "st_f64": 12345.5,
///     "st_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
/// // Subsecond precision in numbers will be rounded away
///
/// let json = json!({
///     "st_f64": 12345.5,
///     "st_string": "12345.987654",
/// });
/// let expected = Timestamps {
///     st_f64: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 500_000_000)).unwrap(),
///     st_string: SystemTime::UNIX_EPOCH.checked_add(Duration::new(12345, 987_654_000)).unwrap(),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`chrono::DateTime<Utc>`][DateTime] and [`chrono::DateTime<Local>`][DateTime] are also supported when using the `chrono` feature.
/// Like [`SystemTime`], it is a signed timestamp, thus can be de/serialized as an `i64`.
///
/// ```rust
/// # #[cfg(all(feature = "macros", feature = "chrono"))] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TimestampSecondsWithFrac};
/// # use chrono_crate::{DateTime, Local, TimeZone, Utc};
/// # /* Ugliness to make the docs look nicer since I want to hide the rename of the chrono crate
/// use chrono::{DateTime, Local, TimeZone, Utc};
/// # */
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Timestamps {
///     #[serde_as(as = "TimestampSecondsWithFrac<f64>")]
///     dt_f64: DateTime<Utc>,
///     #[serde_as(as = "TimestampSecondsWithFrac<String>")]
///     dt_string: DateTime<Local>,
/// };
///
/// // Serialization
///
/// let ts = Timestamps {
///     dt_f64: Utc.timestamp(-12345, 500_000_000),
///     dt_string: Local.timestamp(12345, 999_999_000),
/// };
/// // Observe the different data types
/// let expected = json!({
///     "dt_f64": -12344.5,
///     "dt_string": "12345.999999",
/// });
/// assert_eq!(expected, serde_json::to_value(&ts).unwrap());
///
/// // Deserialization works too
///
/// let json = json!({
///     "dt_f64": -12344.5,
///     "dt_string": "12345.987",
/// });
/// let expected = Timestamps {
///     dt_f64: Utc.timestamp(-12345, 500_000_000),
///     dt_string: Local.timestamp(12345, 987_000_000),
/// };
/// assert_eq!(expected, serde_json::from_value(json).unwrap());
/// # }
/// ```
///
/// [`SystemTime`]: std::time::SystemTime
/// [DateTime]: chrono_crate::DateTime
/// [NaiveDateTime]: chrono_crate::NaiveDateTime
/// [feature flag]: https://docs.rs/serde_with/1.14.0/serde_with/guide/feature_flags/index.html
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSeconds`] with milli-seconds as base unit.
///
/// This type is equivalent to [`TimestampSeconds`] except that each unit represents 1 milli-second instead of 1 second for [`TimestampSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMilliSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSecondsWithFrac`] with milli-seconds as base unit.
///
/// This type is equivalent to [`TimestampSecondsWithFrac`] except that each unit represents 1 milli-second instead of 1 second for [`TimestampSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMilliSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSeconds`] with micro-seconds as base unit.
///
/// This type is equivalent to [`TimestampSeconds`] except that each unit represents 1 micro-second instead of 1 second for [`TimestampSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMicroSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSecondsWithFrac`] with micro-seconds as base unit.
///
/// This type is equivalent to [`TimestampSecondsWithFrac`] except that each unit represents 1 micro-second instead of 1 second for [`TimestampSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampMicroSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSeconds`] with nano-seconds as base unit.
///
/// This type is equivalent to [`TimestampSeconds`] except that each unit represents 1 nano-second instead of 1 second for [`TimestampSeconds`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampNanoSeconds<
    FORMAT: formats::Format = i64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Equivalent to [`TimestampSecondsWithFrac`] with nano-seconds as base unit.
///
/// This type is equivalent to [`TimestampSecondsWithFrac`] except that each unit represents 1 nano-second instead of 1 second for [`TimestampSecondsWithFrac`].
#[derive(Copy, Clone, Debug, Default)]
pub struct TimestampNanoSecondsWithFrac<
    FORMAT: formats::Format = f64,
    STRICTNESS: formats::Strictness = formats::Strict,
>(PhantomData<(FORMAT, STRICTNESS)>);

/// Optimized handling of owned and borrowed byte representations.
///
/// Serialization of byte sequences like `&[u8]` or `Vec<u8>` is quite inefficient since each value will be serialized individually.
/// This converter type optimizes the serialization and deserialization.
///
/// This is a port of the [`serde_bytes`] crate making it compatible with the `serde_as`-annotation, which allows it to be used in more cases than provided by [`serde_bytes`].
///
/// The type provides de/serialization for these types:
///
/// * `[u8; N]`, not possible using `serde_bytes`
/// * `&[u8; N]`, not possible using `serde_bytes`
/// * `&[u8]`
/// * `Box<[u8; N]>`, not possible using `serde_bytes`
/// * `Box<[u8]>`
/// * `Vec<u8>`
/// * `Cow<'_, [u8]>`
/// * `Cow<'_, [u8; N]>`, not possible using `serde_bytes`
///
/// [`serde_bytes`]: https://crates.io/crates/serde_bytes
///
/// # Examples
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::{serde_as, Bytes};
/// # use std::borrow::Cow;
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Test<'a> {
/// #   #[cfg(FALSE)]
///     #[serde_as(as = "Bytes")]
///     array: [u8; 15],
///     #[serde_as(as = "Bytes")]
///     boxed: Box<[u8]>,
///     #[serde_as(as = "Bytes")]
///     #[serde(borrow)]
///     cow: Cow<'a, [u8]>,
/// #   #[cfg(FALSE)]
///     #[serde_as(as = "Bytes")]
///     #[serde(borrow)]
///     cow_array: Cow<'a, [u8; 15]>,
///     #[serde_as(as = "Bytes")]
///     vec: Vec<u8>,
/// }
///
/// let value = Test {
/// #   #[cfg(FALSE)]
///     array: b"0123456789ABCDE".clone(),
///     boxed: b"...".to_vec().into_boxed_slice(),
///     cow: Cow::Borrowed(b"FooBar"),
/// #   #[cfg(FALSE)]
///     cow_array: Cow::Borrowed(&[42u8; 15]),
///     vec: vec![0x41, 0x61, 0x21],
/// };
/// let expected = r#"(
///     array: "MDEyMzQ1Njc4OUFCQ0RF",
///     boxed: "Li4u",
///     cow: "Rm9vQmFy",
///     cow_array: "KioqKioqKioqKioqKioq",
///     vec: "QWEh",
/// )"#;
/// # drop(expected);
/// # // Create a fake expected value that doesn't use const generics
/// # let expected = r#"(
/// #     boxed: "Li4u",
/// #     cow: "Rm9vQmFy",
/// #     vec: "QWEh",
/// # )"#;
///
/// # let pretty_config = ron::ser::PrettyConfig::new()
/// #     .new_line("\n".into());
/// assert_eq!(expected, ron::ser::to_string_pretty(&value, pretty_config).unwrap());
/// assert_eq!(value, ron::from_str(&expected).unwrap());
/// # }
/// ```
///
/// Fully borrowed types can also be used but you'll need a Deserializer that
/// supports Serde's 0-copy deserialization:
///
/// ```
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::{serde_as, Bytes};
/// # use std::borrow::Cow;
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct TestBorrows<'a> {
/// #   #[cfg(FALSE)]
///     #[serde_as(as = "Bytes")]
///     #[serde(borrow)]
///     array_buf: &'a [u8; 15],
///     #[serde_as(as = "Bytes")]
///     #[serde(borrow)]
///     buf: &'a [u8],
/// }
///
/// let value = TestBorrows {
/// #   #[cfg(FALSE)]
///     array_buf: &[10u8; 15],
///     buf: &[20u8, 21u8, 22u8],
/// };
/// let expected = r#"(
///     array_buf: "CgoKCgoKCgoKCgoKCgoK",
///     buf: "FBUW",
/// )"#;
/// # drop(expected);
/// # // Create a fake expected value that doesn't use const generics
/// # let expected = r#"(
/// #     buf: "FBUW",
/// # )"#;
///
/// # let pretty_config = ron::ser::PrettyConfig::new()
/// #     .new_line("\n".into());
/// assert_eq!(expected, ron::ser::to_string_pretty(&value, pretty_config).unwrap());
/// // RON doesn't support borrowed deserialization of byte arrays
/// # }
/// ```
///
/// ## Alternative to [`BytesOrString`]
///
/// The [`Bytes`] can replace [`BytesOrString`].
/// [`Bytes`] is implemented for more types, which makes it better.
/// The serialization behavior of [`Bytes`] differs from [`BytesOrString`], therefore only `deserialize_as` should be used.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_json::json;
/// # use serde_with::{serde_as, Bytes};
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, serde::Serialize)]
/// struct Test {
///     #[serde_as(deserialize_as = "Bytes")]
///     from_bytes: Vec<u8>,
///     #[serde_as(deserialize_as = "Bytes")]
///     from_str: Vec<u8>,
/// }
///
/// // Different serialized values ...
/// let j = json!({
///     "from_bytes": [70,111,111,45,66,97,114],
///     "from_str": "Foo-Bar",
/// });
///
/// // can be deserialized ...
/// let test = Test {
///     from_bytes: b"Foo-Bar".to_vec(),
///     from_str: b"Foo-Bar".to_vec(),
/// };
/// assert_eq!(test, serde_json::from_value(j).unwrap());
///
/// // and serialization will always be a byte sequence
/// # assert_eq!(json!(
/// {
///     "from_bytes": [70,111,111,45,66,97,114],
///     "from_str": [70,111,111,45,66,97,114],
/// }
/// # ), serde_json::to_value(&test).unwrap());
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct Bytes;

/// Deserialize one or many elements
///
/// Sometimes it is desirable to have a shortcut in writing 1-element lists in a config file.
/// Usually, this is done by either writing a list or the list element itself.
/// This distinction is not semantically important on the Rust side, thus both forms should deserialize into the same `Vec`.
///
/// The `OneOrMany` adapter achieves exactly this use case.
/// The serialization behavior can be tweaked to either always serialize as a list using `PreferMany` or to serialize as the inner element if possible using `PreferOne`.
/// By default, `PreferOne` is assumed, which can also be omitted like `OneOrMany<_>`.
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_json::json;
/// # use serde_with::{serde_as, OneOrMany};
/// # use serde_with::formats::{PreferOne, PreferMany};
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, serde::Serialize)]
/// struct Data {
///     #[serde_as(deserialize_as = "OneOrMany<_, PreferOne>")]
///     countries: Vec<String>,
///     #[serde_as(deserialize_as = "OneOrMany<_, PreferMany>")]
///     cities: Vec<String>,
/// }
///
/// // The adapter allows deserializing a `Vec` from either
/// // a single element
/// let j = json!({
///     "countries": "Spain",
///     "cities": "Berlin",
/// });
/// assert!(serde_json::from_value::<Data>(j).is_ok());
///
/// // or from a list.
/// let j = json!({
///     "countries": ["Germany", "France"],
///     "cities": ["Amsterdam"],
/// });
/// assert!(serde_json::from_value::<Data>(j).is_ok());
///
/// // For serialization you can choose how a single element should be encoded.
/// // Either directly, with `PreferOne` (default), or as a list with `PreferMany`.
/// let data = Data {
///     countries: vec!["Spain".to_string()],
///     cities: vec!["Berlin".to_string()],
/// };
/// let j = json!({
///     "countries": "Spain",
///     "cities": ["Berlin"],
/// });
/// assert_eq!(data, serde_json::from_value(j).unwrap());
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct OneOrMany<T, FORMAT: formats::Format = formats::PreferOne>(PhantomData<(T, FORMAT)>);

/// Try multiple deserialization options until one succeeds.
///
/// This adapter allows you to specify a list of deserialization options.
/// They are tried in order and the first one working is applied.
/// Serialization always picks the first option.
///
/// `PickFirst` has one type parameter which must be instantiated with a tuple of two, three, or four elements.
/// For example, `PickFirst<(_, DisplayFromStr)>` on a field of type `u32` allows deserializing from a number or from a string via the `FromStr` trait.
/// The value will be serialized as a number, since that is what the first type `_` indicates.
///
/// # Examples
///
/// Deserialize a number from either a number or a string.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, DisplayFromStr, PickFirst};
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Data {
///     #[serde_as(as = "PickFirst<(_, DisplayFromStr)>")]
///     as_number: u32,
///     #[serde_as(as = "PickFirst<(DisplayFromStr, _)>")]
///     as_string: u32,
/// }
/// let data = Data {
///     as_number: 123,
///     as_string: 456
/// };
///
/// // Both fields can be deserialized from numbers:
/// let j = json!({
///     "as_number": 123,
///     "as_string": 456,
/// });
/// assert_eq!(data, serde_json::from_value(j).unwrap());
///
/// // or from a string:
/// let j = json!({
///     "as_number": "123",
///     "as_string": "456",
/// });
/// assert_eq!(data, serde_json::from_value(j).unwrap());
///
/// // For serialization the first type in the tuple determines the behavior.
/// // The `as_number` field will use the normal `Serialize` behavior and produce a number,
/// // while `as_string` used `Display` to produce a string.
/// let expected = json!({
///     "as_number": 123,
///     "as_string": "456",
/// });
/// assert_eq!(expected, serde_json::to_value(&data).unwrap());
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct PickFirst<T>(PhantomData<T>);

/// Serialize value by converting to/from a proxy type with serde support.
///
/// This adapter serializes a type `O` by converting it into a second type `T` and serializing `T`.
/// Deserializing works analogue, by deserializing a `T` and then converting into `O`.
///
/// ```rust
/// # #[cfg(FALSE)] {
/// struct S {
///     #[serde_as(as = "FromInto<T>")]
///     value: O,
/// }
/// # }
/// ```
///
/// For serialization `O` needs to be `O: Into<T> + Clone`.
/// For deserialization the opposite `T: Into<O>` is required.
/// The `Clone` bound is required since `serialize` operates on a reference but `Into` implementations on references are uncommon.
///
/// **Note**: [`TryFromInto`] is the more generalized version of this adapter which uses the [`TryInto`](std::convert::TryInto) trait instead.
///
/// # Example
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, FromInto};
/// #
/// #[derive(Clone, Debug, PartialEq)]
/// struct Rgb {
///     red: u8,
///     green: u8,
///     blue: u8,
/// }
///
/// # /*
/// impl From<(u8, u8, u8)> for Rgb { ... }
/// impl From<Rgb> for (u8, u8, u8) { ... }
/// # */
/// #
/// # impl From<(u8, u8, u8)> for Rgb {
/// #     fn from(v: (u8, u8, u8)) -> Self {
/// #         Rgb {
/// #             red: v.0,
/// #             green: v.1,
/// #             blue: v.2,
/// #         }
/// #     }
/// # }
/// #
/// # impl From<Rgb> for (u8, u8, u8) {
/// #     fn from(v: Rgb) -> Self {
/// #         (v.red, v.green, v.blue)
/// #     }
/// # }
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Color {
///     #[serde_as(as = "FromInto<(u8, u8, u8)>")]
///     rgb: Rgb,
/// }
/// let color = Color {
///     rgb: Rgb {
///         red: 128,
///         green: 64,
///         blue: 32,
///     },
/// };
///
/// // Define our expected JSON form
/// let j = json!({
///     "rgb": [128, 64, 32],
/// });
/// // Ensure serialization and deserialization produce the expected results
/// assert_eq!(j, serde_json::to_value(&color).unwrap());
/// assert_eq!(color, serde_json::from_value(j).unwrap());
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct FromInto<T>(PhantomData<T>);

/// Serialize value by converting to/from a proxy type with serde support.
///
/// This adapter serializes a type `O` by converting it into a second type `T` and serializing `T`.
/// Deserializing works analogue, by deserializing a `T` and then converting into `O`.
///
/// ```rust
/// # #[cfg(FALSE)] {
/// struct S {
///     #[serde_as(as = "TryFromInto<T>")]
///     value: O,
/// }
/// # }
/// ```
///
/// For serialization `O` needs to be `O: TryInto<T> + Clone`.
/// For deserialization the opposite `T: TryInto<O>` is required.
/// The `Clone` bound is required since `serialize` operates on a reference but `TryInto` implementations on references are uncommon.
/// In both cases the `TryInto::Error` type must implement [`Display`](std::fmt::Display).
///
/// **Note**: [`FromInto`] is the more specialized version of this adapter which uses the infallible [`Into`] trait instead.
/// [`TryFromInto`] is strictly more general and can also be used where [`FromInto`] is applicable.
/// The example shows a use case, when only the deserialization behavior is fallible, but not serializing.
///
/// # Example
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, TryFromInto};
/// # use std::convert::TryFrom;
/// #
/// #[derive(Clone, Debug, PartialEq)]
/// enum Boollike {
///     True,
///     False,
/// }
///
/// # /*
/// impl From<Boollike> for u8 { ... }
/// # */
/// #
/// impl TryFrom<u8> for Boollike {
///     type Error = String;
///     fn try_from(v: u8) -> Result<Self, Self::Error> {
///         match v {
///             0 => Ok(Boollike::False),
///             1 => Ok(Boollike::True),
///             _ => Err(format!("Boolikes can only be constructed from 0 or 1 but found {}", v))
///         }
///     }
/// }
/// #
/// # impl From<Boollike> for u8 {
/// #     fn from(v: Boollike) -> Self {
/// #        match v {
/// #            Boollike::True => 1,
/// #            Boollike::False => 0,
/// #        }
/// #     }
/// # }
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Data {
///     #[serde_as(as = "TryFromInto<u8>")]
///     b: Boollike,
/// }
/// let data = Data {
///     b: Boollike::True,
/// };
///
/// // Define our expected JSON form
/// let j = json!({
///     "b": 1,
/// });
/// // Ensure serialization and deserialization produce the expected results
/// assert_eq!(j, serde_json::to_value(&data).unwrap());
/// assert_eq!(data, serde_json::from_value(j).unwrap());
///
/// // Numbers besides 0 or 1 should be an error
/// let j = json!({
///     "b": 2,
/// });
/// assert_eq!("Boolikes can only be constructed from 0 or 1 but found 2", serde_json::from_value::<Data>(j).unwrap_err().to_string());
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct TryFromInto<T>(PhantomData<T>);

/// Borrow `Cow` data during deserialization when possible.
///
/// The types `Cow<'a, [u8]>`, `Cow<'a, [u8; N]>`, and `Cow<'a, str>` can borrow from the input data during deserialization.
/// serde supports this, by annotating the fields with `#[serde(borrow)]`. but does not support borrowing on nested types.
/// This gap is filled by this `BorrowCow` adapter.
///
/// Using this adapter with `Cow<'a, [u8]>`/Cow<'a, [u8; N]>` will serialize the value as a sequence of `u8` values.
/// This *might* not allow to borrow the data during deserialization.
/// For a different format, which is also more efficient, use the [`Bytes`] adapter, which is also implemented for `Cow`.
///
/// When combined with the [`serde_as`] attribute, the `#[serde(borrow)]` annotation will be added automatically.
/// If the annotation is wrong or too broad, for example because of multiple lifetime parameters, a manual annotation is required.
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::{serde_as, BorrowCow};
/// # use std::borrow::Cow;
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Data<'a, 'b, 'c> {
///     #[serde_as(as = "BorrowCow")]
///     str: Cow<'a, str>,
///     #[serde_as(as = "BorrowCow")]
///     slice: Cow<'b, [u8]>,
///
///     #[serde_as(as = "Option<[BorrowCow; 1]>")]
///     nested: Option<[Cow<'c, str>; 1]>,
/// }
/// let data = Data {
///     str: "foobar".into(),
///     slice: b"foobar"[..].into(),
///     nested: Some(["HelloWorld".into()]),
/// };
///
/// // Define our expected JSON form
/// let j = r#"{
///   "str": "foobar",
///   "slice": [
///     102,
///     111,
///     111,
///     98,
///     97,
///     114
///   ],
///   "nested": [
///     "HelloWorld"
///   ]
/// }"#;
/// // Ensure serialization and deserialization produce the expected results
/// assert_eq!(j, serde_json::to_string_pretty(&data).unwrap());
/// assert_eq!(data, serde_json::from_str(j).unwrap());
///
/// // Cow borrows from the input data
/// let deserialized: Data<'_, '_, '_> = serde_json::from_str(j).unwrap();
/// assert!(matches!(deserialized.str, Cow::Borrowed(_)));
/// assert!(matches!(deserialized.nested, Some([Cow::Borrowed(_)])));
/// // JSON does not allow borrowing bytes, so `slice` does not borrow
/// assert!(matches!(deserialized.slice, Cow::Owned(_)));
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct BorrowCow;

/// Deserialize a sequence into `Vec<T>`, skipping elements which fail to deserialize.
///
/// The serialization behavior is identical to `Vec<T>`. This is an alternative to `Vec<T>`
/// which is resilient against unexpected data.
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::{serde_as, VecSkipError};
/// #
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// # #[non_exhaustive]
/// enum Color {
///     Red,
///     Green,
///     Blue,
/// }
/// # use Color::*;
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Palette(#[serde_as(as = "VecSkipError<_>")] Vec<Color>);
///
/// let data = Palette(vec![Blue, Green,]);
/// let source_json = r#"["Blue", "Yellow", "Green"]"#;
/// let data_json = r#"["Blue","Green"]"#;
/// // Ensure serialization and deserialization produce the expected results
/// assert_eq!(data_json, serde_json::to_string(&data).unwrap());
/// assert_eq!(data, serde_json::from_str(source_json).unwrap());
/// # }
/// ```

#[derive(Copy, Clone, Debug, Default)]
pub struct VecSkipError<T>(PhantomData<T>);

/// Deserialize a boolean from a number
///
/// Deserialize a number (of `u8`) and turn it into a boolean.
/// The adapter supports a [`Strict`](crate::formats::Strict) and [`Flexible`](crate::formats::Flexible) format.
/// In `Strict` mode, the number must be `0` or `1`.
/// All other values produce an error.
/// In `Flexible` mode, the number any non-zero value is converted to `true`.
///
/// During serialization only `0` or `1` are ever emitted.
///
/// # Examples
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::{serde_as, BoolFromInt};
/// #
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Data(#[serde_as(as = "BoolFromInt")] bool);
///
/// let data = Data(true);
/// let j = json!(1);
/// // Ensure serialization and deserialization produce the expected results
/// assert_eq!(j, serde_json::to_value(&data).unwrap());
/// assert_eq!(data, serde_json::from_value(j).unwrap());
///
/// // false maps to 0
/// let data = Data(false);
/// let j = json!(0);
/// assert_eq!(j, serde_json::to_value(&data).unwrap());
/// assert_eq!(data, serde_json::from_value(j).unwrap());
//
/// #[serde_as]
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Flexible(#[serde_as(as = "BoolFromInt<serde_with::formats::Flexible>")] bool);
///
/// // Flexible turns any non-zero number into true
/// let data = Flexible(true);
/// let j = json!(100);
/// assert_eq!(data, serde_json::from_value(j).unwrap());
/// # }
/// ```
#[derive(Copy, Clone, Debug, Default)]
pub struct BoolFromInt<S: formats::Strictness = formats::Strict>(PhantomData<S>);
