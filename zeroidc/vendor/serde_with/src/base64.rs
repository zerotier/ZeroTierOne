//! De/Serialization of base64 encoded bytes
//!
//! This modules is only available when using the `base64` feature of the crate.
//!
//! Please check the documentation on the [`Base64`] type for details.

use crate::{formats, DeserializeAs, SerializeAs};
use alloc::{format, string::String, vec::Vec};
use core::{
    convert::{TryFrom, TryInto},
    default::Default,
    marker::PhantomData,
};
use serde::{de::Error, Deserialize, Deserializer, Serialize, Serializer};

/// Serialize bytes with base64
///
/// The type serializes a sequence of bytes as a base64 string.
/// It works on any type implementing `AsRef<[u8]>` for serialization and `TryFrom<Vec<u8>>` for deserialization.
///
/// The type allows customizing the character set and the padding behavior.
/// The `CHARSET` is a type implementing [`CharacterSet`].
/// `PADDING` specifies if serializing should emit padding.
/// Deserialization always supports padded and unpadded formats.
/// [`formats::Padded`] emits padding and [`formats::Unpadded`] leaves it off.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::serde_as;
/// use serde_with::base64::{Base64, Bcrypt, BinHex, Standard};
/// use serde_with::formats::{Padded, Unpadded};
///
/// #[serde_as]
/// # #[derive(Debug, PartialEq, Eq)]
/// #[derive(Serialize, Deserialize)]
/// struct B64 {
///     // The default is the same as Standard character set with padding
///     #[serde_as(as = "Base64")]
///     default: Vec<u8>,
///     // Only change the character set, implies padding
///     #[serde_as(as = "Base64<BinHex>")]
///     charset_binhex: Vec<u8>,
///
///     #[serde_as(as = "Base64<Standard, Padded>")]
///     explicit_padding: Vec<u8>,
///     #[serde_as(as = "Base64<Bcrypt, Unpadded>")]
///     no_padding: Vec<u8>,
/// }
///
/// let b64 = B64 {
///     default: b"Hello World".to_vec(),
///     charset_binhex: b"Hello World".to_vec(),
///     explicit_padding: b"Hello World".to_vec(),
///     no_padding: b"Hello World".to_vec(),
/// };
/// let json = serde_json::json!({
///     "default": "SGVsbG8gV29ybGQ=",
///     "charset_binhex": "5'8VD'mI8epaD'3=",
///     "explicit_padding": "SGVsbG8gV29ybGQ=",
///     "no_padding": "QETqZE6eT07wZEO",
/// });
///
/// // Test serialization and deserialization
/// assert_eq!(json, serde_json::to_value(&b64).unwrap());
/// assert_eq!(b64, serde_json::from_value(json).unwrap());
/// # }
/// ```

// The padding might be better as `const PADDING: bool = true`
// https://blog.rust-lang.org/inside-rust/2021/09/06/Splitting-const-generics.html#featureconst_generics_default/
#[derive(Copy, Clone, Debug, Default)]
pub struct Base64<CHARSET: CharacterSet = Standard, PADDING: formats::Format = formats::Padded>(
    PhantomData<(CHARSET, PADDING)>,
);

impl<T, CHARSET> SerializeAs<T> for Base64<CHARSET, formats::Padded>
where
    T: AsRef<[u8]>,
    CHARSET: CharacterSet,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        base64_crate::encode_config(source, base64_crate::Config::new(CHARSET::charset(), true))
            .serialize(serializer)
    }
}

impl<T, CHARSET> SerializeAs<T> for Base64<CHARSET, formats::Unpadded>
where
    T: AsRef<[u8]>,
    CHARSET: CharacterSet,
{
    fn serialize_as<S>(source: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        base64_crate::encode_config(source, base64_crate::Config::new(CHARSET::charset(), false))
            .serialize(serializer)
    }
}

impl<'de, T, CHARSET, FORMAT> DeserializeAs<'de, T> for Base64<CHARSET, FORMAT>
where
    T: TryFrom<Vec<u8>>,
    CHARSET: CharacterSet,
    FORMAT: formats::Format,
{
    fn deserialize_as<D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
    {
        String::deserialize(deserializer)
            .and_then(|s| {
                base64_crate::decode_config(
                    &*s,
                    base64_crate::Config::new(CHARSET::charset(), false),
                )
                .map_err(Error::custom)
            })
            .and_then(|vec: Vec<u8>| {
                let length = vec.len();
                vec.try_into().map_err(|_e: T::Error| {
                    Error::custom(format!(
                        "Can't convert a Byte Vector of length {} to the output type.",
                        length
                    ))
                })
            })
    }
}

/// A base64 character set from [this list](base64_crate::CharacterSet).
pub trait CharacterSet {
    /// Return a specific character set.
    ///
    /// Return one enum variant of the [`base64::CharacterSet`](base64_crate::CharacterSet) enum.
    fn charset() -> base64_crate::CharacterSet;
}

/// The standard character set (uses `+` and `/`).
///
/// See [RFC 3548](https://tools.ietf.org/html/rfc3548#section-3).
#[derive(Copy, Clone, Debug, Default)]
pub struct Standard;
impl CharacterSet for Standard {
    fn charset() -> base64_crate::CharacterSet {
        base64_crate::CharacterSet::Standard
    }
}

/// The URL safe character set (uses `-` and `_`).
///
/// See [RFC 3548](https://tools.ietf.org/html/rfc3548#section-3).
#[derive(Copy, Clone, Debug, Default)]
pub struct UrlSafe;
impl CharacterSet for UrlSafe {
    fn charset() -> base64_crate::CharacterSet {
        base64_crate::CharacterSet::UrlSafe
    }
}

/// The `crypt(3)` character set (uses `./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`).
///
/// Not standardized, but folk wisdom on the net asserts that this alphabet is what crypt uses.
#[derive(Copy, Clone, Debug, Default)]
pub struct Crypt;
impl CharacterSet for Crypt {
    fn charset() -> base64_crate::CharacterSet {
        base64_crate::CharacterSet::Crypt
    }
}

/// The bcrypt character set (uses `./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`).
#[derive(Copy, Clone, Debug, Default)]
pub struct Bcrypt;
impl CharacterSet for Bcrypt {
    fn charset() -> base64_crate::CharacterSet {
        base64_crate::CharacterSet::Bcrypt
    }
}

/// The character set used in IMAP-modified UTF-7 (uses `+` and `,`).
///
/// See [RFC 3501](https://tools.ietf.org/html/rfc3501#section-5.1.3).
#[derive(Copy, Clone, Debug, Default)]
pub struct ImapMutf7;
impl CharacterSet for ImapMutf7 {
    fn charset() -> base64_crate::CharacterSet {
        base64_crate::CharacterSet::ImapMutf7
    }
}

/// The character set used in BinHex 4.0 files.
///
/// See [BinHex 4.0 Definition](http://files.stairways.com/other/binhex-40-specs-info.txt).
#[derive(Copy, Clone, Debug, Default)]
pub struct BinHex;
impl CharacterSet for BinHex {
    fn charset() -> base64_crate::CharacterSet {
        base64_crate::CharacterSet::BinHex
    }
}
