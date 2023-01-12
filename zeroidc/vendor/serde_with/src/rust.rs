//! De/Serialization for Rust's builtin and std types

use crate::{utils, Separator};
#[cfg(doc)]
use alloc::collections::BTreeMap;
use alloc::{
    string::{String, ToString},
    vec::Vec,
};
use core::{
    cmp::Eq,
    fmt::{self, Display},
    hash::Hash,
    iter::FromIterator,
    marker::PhantomData,
    str::FromStr,
};
use serde::{
    de::{Deserialize, DeserializeOwned, Deserializer, Error, MapAccess, SeqAccess, Visitor},
    ser::{Serialize, Serializer},
};
#[cfg(doc)]
use std::collections::HashMap;

/// De/Serialize using [`Display`] and [`FromStr`] implementation
///
/// This allows deserializing a string as a number.
/// It can be very useful for serialization formats like JSON, which do not support integer
/// numbers and have to resort to strings to represent them.
///
/// If you control the type you want to de/serialize, you can instead use the two derive macros, [`SerializeDisplay`] and [`DeserializeFromStr`].
/// They properly implement the traits [`Serialize`] and [`Deserialize`] such that user of the type no longer have to use the with-attribute.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed via [`DisplayFromStr`] and using the [`serde_as`] macro.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct A {
///     #[serde_as(as = "DisplayFromStr")]
///     value: mime::Mime,
/// }
/// # }
/// ```
///
/// # Examples
///
/// ```rust
/// # use serde::{Deserialize, Serialize};
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::display_fromstr")]
///     mime: mime::Mime,
///     #[serde(with = "serde_with::rust::display_fromstr")]
///     number: u32,
/// }
///
/// let v: A = serde_json::from_str(r#"{
///     "mime": "text/plain",
///     "number": "159"
/// }"#).unwrap();
/// assert_eq!(mime::TEXT_PLAIN, v.mime);
/// assert_eq!(159, v.number);
///
/// let x = A {
///     mime: mime::STAR_STAR,
///     number: 777,
/// };
/// assert_eq!(
///     r#"{"mime":"*/*","number":"777"}"#,
///     serde_json::to_string(&x).unwrap()
/// );
/// ```
///
/// [`DeserializeFromStr`]: serde_with_macros::DeserializeFromStr
/// [`DisplayFromStr`]: crate::DisplayFromStr
/// [`serde_as`]: crate::guide::serde_as
/// [`SerializeDisplay`]: serde_with_macros::SerializeDisplay
pub mod display_fromstr {
    use super::*;

    /// Deserialize T using [`FromStr`]
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromStr,
        T::Err: Display,
    {
        struct Helper<S>(PhantomData<S>);

        impl<'de, S> Visitor<'de> for Helper<S>
        where
            S: FromStr,
            <S as FromStr>::Err: Display,
        {
            type Value = S;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(formatter, "a string")
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                value.parse::<Self::Value>().map_err(Error::custom)
            }
        }

        deserializer.deserialize_str(Helper(PhantomData))
    }

    /// Serialize T using [Display]
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Display,
        S: Serializer,
    {
        serializer.collect_str(&value)
    }
}

/// De/Serialize sequences using [`FromIterator`] and [`IntoIterator`] implementation for it and [`Display`] and [`FromStr`] implementation for each element
///
/// This allows to serialize and deserialize collections with elements which can be represented as strings.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed via [`DisplayFromStr`] and using the [`serde_as`] macro.
/// Instead of
///
/// ```rust,ignore
/// #[serde(with = "serde_with::rust::seq_display_fromstr")]
/// addresses: BTreeSet<Ipv4Addr>,
/// ```
/// you can write:
/// ```rust,ignore
/// #[serde_as(as = "BTreeSet<DisplayFromStr>")]
/// addresses: BTreeSet<Ipv4Addr>,
/// ```
///
/// This works for any container type, so also for `Vec`:
/// ```rust,ignore
/// #[serde_as(as = "Vec<DisplayFromStr>")]
/// bs: Vec<bool>,
/// ```
///
/// # Examples
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// #
/// use std::collections::BTreeSet;
/// use std::net::Ipv4Addr;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::seq_display_fromstr")]
///     addresses: BTreeSet<Ipv4Addr>,
///     #[serde(with = "serde_with::rust::seq_display_fromstr")]
///     bs: Vec<bool>,
/// }
///
/// let v: A = serde_json::from_str(r#"{
///     "addresses": ["192.168.2.1", "192.168.2.2", "192.168.1.1", "192.168.2.2"],
///     "bs": ["true", "false"]
/// }"#).unwrap();
/// assert_eq!(v.addresses.len(), 3);
/// assert!(v.addresses.contains(&Ipv4Addr::new(192, 168, 2, 1)));
/// assert!(v.addresses.contains(&Ipv4Addr::new(192, 168, 2, 2)));
/// assert!(!v.addresses.contains(&Ipv4Addr::new(192, 168, 1, 2)));
/// assert_eq!(v.bs.len(), 2);
/// assert!(v.bs[0]);
/// assert!(!v.bs[1]);
///
/// let x = A {
///     addresses: vec![
///         Ipv4Addr::new(127, 53, 0, 1),
///         Ipv4Addr::new(127, 53, 1, 1),
///         Ipv4Addr::new(127, 53, 0, 2)
///     ].into_iter().collect(),
///     bs: vec![false, true],
/// };
/// assert_eq!(
///     r#"{"addresses":["127.53.0.1","127.53.0.2","127.53.1.1"],"bs":["false","true"]}"#,
///     serde_json::to_string(&x).unwrap()
/// );
/// ```
///
/// [`DisplayFromStr`]: crate::DisplayFromStr
/// [`serde_as`]: crate::guide::serde_as
pub mod seq_display_fromstr {
    use super::*;

    /// Deserialize collection T using [FromIterator] and [FromStr] for each element
    pub fn deserialize<'de, D, T, I>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromIterator<I> + Sized,
        I: FromStr,
        I::Err: Display,
    {
        struct Helper<S>(PhantomData<S>);

        impl<'de, S> Visitor<'de> for Helper<S>
        where
            S: FromStr,
            <S as FromStr>::Err: Display,
        {
            type Value = Vec<S>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(formatter, "a sequence")
            }

            fn visit_seq<A>(self, seq: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                utils::SeqIter::new(seq)
                    .map(|res| {
                        res.and_then(|value: &str| value.parse::<S>().map_err(Error::custom))
                    })
                    .collect()
            }
        }

        deserializer
            .deserialize_seq(Helper(PhantomData))
            .map(T::from_iter)
    }

    /// Serialize collection T using [IntoIterator] and [Display] for each element
    pub fn serialize<S, T, I>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        for<'a> &'a T: IntoIterator<Item = &'a I>,
        I: Display,
    {
        struct SerializeString<'a, I>(&'a I);

        impl<'a, I> Serialize for SerializeString<'a, I>
        where
            I: Display,
        {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                serializer.collect_str(self.0)
            }
        }

        serializer.collect_seq(value.into_iter().map(SerializeString))
    }
}

/// De/Serialize a delimited collection using [`Display`] and [`FromStr`] implementation
///
/// You can define an arbitrary separator, by specifying a type which implements [`Separator`].
/// Some common ones, like space and comma are already predefined and you can find them [here][Separator].
///
/// An empty string deserializes as an empty collection.
///
/// ## Converting to `serde_as`
///
/// The same functionality can also be expressed using the [`serde_as`] macro.
/// The usage is slightly different.
/// `StringWithSeparator` takes a second type, which needs to implement [`Display`]+[`FromStr`] and constitutes the inner type of the collection.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, SpaceSeparator, StringWithSeparator};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct A {
///     #[serde_as(as = "StringWithSeparator::<SpaceSeparator, String>")]
///     tags: Vec<String>,
/// }
/// # }
/// ```
///
/// # Examples
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// #
/// use serde_with::{CommaSeparator, SpaceSeparator};
/// use std::collections::BTreeSet;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::StringWithSeparator::<SpaceSeparator>")]
///     tags: Vec<String>,
///     #[serde(with = "serde_with::rust::StringWithSeparator::<CommaSeparator>")]
///     more_tags: BTreeSet<String>,
/// }
///
/// let v: A = serde_json::from_str(r##"{
///     "tags": "#hello #world",
///     "more_tags": "foo,bar,bar"
/// }"##).unwrap();
/// assert_eq!(vec!["#hello", "#world"], v.tags);
/// assert_eq!(2, v.more_tags.len());
///
/// let x = A {
///     tags: vec!["1".to_string(), "2".to_string(), "3".to_string()],
///     more_tags: BTreeSet::new(),
/// };
/// assert_eq!(
///     r#"{"tags":"1 2 3","more_tags":""}"#,
///     serde_json::to_string(&x).unwrap()
/// );
/// ```
///
/// [`serde_as`]: crate::guide::serde_as
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default)]
pub struct StringWithSeparator<Sep, T = ()>(PhantomData<(Sep, T)>);

impl<Sep> StringWithSeparator<Sep>
where
    Sep: Separator,
{
    /// Serialize collection into a string with separator symbol
    pub fn serialize<S, T, V>(values: T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: IntoIterator<Item = V>,
        V: Display,
    {
        let mut s = String::new();
        for v in values {
            s.push_str(&*v.to_string());
            s.push_str(Sep::separator());
        }
        serializer.serialize_str(if !s.is_empty() {
            // remove trailing separator if present
            &s[..s.len() - Sep::separator().len()]
        } else {
            &s[..]
        })
    }

    /// Deserialize a collection from a string with separator symbol
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromIterator<V>,
        V: FromStr,
        V::Err: Display,
    {
        let s = String::deserialize(deserializer)?;
        if s.is_empty() {
            Ok(None.into_iter().collect())
        } else {
            s.split(Sep::separator())
                .map(FromStr::from_str)
                .collect::<Result<_, _>>()
                .map_err(Error::custom)
        }
    }
}

/// Makes a distinction between a missing, unset, or existing value
///
/// Some serialization formats make a distinction between missing fields, fields with a `null`
/// value, and existing values. One such format is JSON. By default it is not easily possible to
/// differentiate between a missing value and a field which is `null`, as they deserialize to the
/// same value. This helper changes it, by using an `Option<Option<T>>` to deserialize into.
///
/// * `None`: Represents a missing value.
/// * `Some(None)`: Represents a `null` value.
/// * `Some(Some(value))`: Represents an existing value.
///
/// # Examples
///
/// ```rust
/// # use serde::{Deserialize, Serialize};
/// #
/// # #[derive(Debug, PartialEq, Eq)]
/// #[derive(Deserialize, Serialize)]
/// struct Doc {
///     #[serde(
///         default,                                    // <- important for deserialization
///         skip_serializing_if = "Option::is_none",    // <- important for serialization
///         with = "::serde_with::rust::double_option",
///     )]
///     a: Option<Option<u8>>,
/// }
/// // Missing Value
/// let s = r#"{}"#;
/// assert_eq!(Doc { a: None }, serde_json::from_str(s).unwrap());
/// assert_eq!(s, serde_json::to_string(&Doc { a: None }).unwrap());
///
/// // Unset Value
/// let s = r#"{"a":null}"#;
/// assert_eq!(Doc { a: Some(None) }, serde_json::from_str(s).unwrap());
/// assert_eq!(s, serde_json::to_string(&Doc { a: Some(None) }).unwrap());
///
/// // Existing Value
/// let s = r#"{"a":5}"#;
/// assert_eq!(Doc { a: Some(Some(5)) }, serde_json::from_str(s).unwrap());
/// assert_eq!(s, serde_json::to_string(&Doc { a: Some(Some(5)) }).unwrap());
/// ```
#[allow(clippy::option_option)]
pub mod double_option {
    use super::*;

    /// Deserialize potentially non-existing optional value
    pub fn deserialize<'de, T, D>(deserializer: D) -> Result<Option<Option<T>>, D::Error>
    where
        T: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(Some)
    }

    /// Serialize optional value
    pub fn serialize<S, T>(values: &Option<Option<T>>, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: Serialize,
    {
        match values {
            None => serializer.serialize_unit(),
            Some(None) => serializer.serialize_none(),
            Some(Some(v)) => serializer.serialize_some(&v),
        }
    }
}

/// Serialize inner value if [`Some`]`(T)`. If [`None`], serialize the unit struct `()`.
///
/// When used in conjunction with `skip_serializing_if = "Option::is_none"` and
/// `default`, you can build an optional value by skipping if it is [`None`], or serializing its
/// inner value if [`Some`]`(T)`.
///
/// Not all serialization formats easily support optional values.
/// While JSON uses the [`Option`] type to represent optional values and only serializes the inner
/// part of the [`Some`]`()`, other serialization formats, such as [RON][], choose to serialize the
/// [`Some`] around a value.
/// This helper helps building a truly optional value for such serializers.
///
/// [RON]: https://github.com/ron-rs/ron
///
/// # Example
///
/// ```rust
/// # use serde::{Deserialize, Serialize};
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize, Serialize)]
/// struct Doc {
///     mandatory: usize,
///     #[serde(
///         default,                                    // <- important for deserialization
///         skip_serializing_if = "Option::is_none",    // <- important for serialization
///         with = "::serde_with::rust::unwrap_or_skip",
///     )]
///     optional: Option<usize>,
/// }
///
/// // Transparently add/remove Some() wrapper
/// # let pretty_config = ron::ser::PrettyConfig::new()
/// #     .new_line("\n".into());
/// let s = r#"(
///     mandatory: 1,
///     optional: 2,
/// )"#;
/// let v = Doc {
///     mandatory: 1,
///     optional: Some(2),
/// };
/// assert_eq!(v, ron::de::from_str(s).unwrap());
/// assert_eq!(s, ron::ser::to_string_pretty(&v, pretty_config).unwrap());
///
/// // Missing values are deserialized as `None`
/// // while `None` values are skipped during serialization.
/// # let pretty_config = ron::ser::PrettyConfig::new()
/// #     .new_line("\n".into());
/// let s = r#"(
///     mandatory: 1,
/// )"#;
/// let v = Doc {
///     mandatory: 1,
///     optional: None,
/// };
/// assert_eq!(v, ron::de::from_str(s).unwrap());
/// assert_eq!(s, ron::ser::to_string_pretty(&v, pretty_config).unwrap());
/// ```
pub mod unwrap_or_skip {
    use super::*;

    /// Deserialize value wrapped in Some(T)
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<Option<T>, D::Error>
    where
        D: Deserializer<'de>,
        T: DeserializeOwned,
    {
        T::deserialize(deserializer).map(Some)
    }

    /// Serialize value if Some(T), unit struct if None
    pub fn serialize<T, S>(option: &Option<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        if let Some(value) = option {
            value.serialize(serializer)
        } else {
            ().serialize(serializer)
        }
    }
}

/// Ensure no duplicate values exist in a set.
///
/// By default serde has a last-value-wins implementation, if duplicate values for a set exist.
/// Sometimes it is desirable to know when such an event happens, as the first value is overwritten
/// and it can indicate an error in the serialized data.
///
/// This helper returns an error if two identical values exist in a set.
///
/// The implementation supports both the [`HashSet`] and the [`BTreeSet`] from the standard library.
///
/// [`HashSet`]: std::collections::HashSet
/// [`BTreeSet`]: std::collections::HashSet
///
/// # Example
///
/// ```rust
/// # use std::{collections::HashSet, iter::FromIterator};
/// # use serde::Deserialize;
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize)]
/// struct Doc {
///     #[serde(with = "::serde_with::rust::sets_duplicate_value_is_error")]
///     set: HashSet<usize>,
/// }
///
/// // Sets are serialized normally,
/// let s = r#"{"set": [1, 2, 3, 4]}"#;
/// let v = Doc {
///     set: HashSet::from_iter(vec![1, 2, 3, 4]),
/// };
/// assert_eq!(v, serde_json::from_str(s).unwrap());
///
/// // but create an error if duplicate values, like the `1`, exist.
/// let s = r#"{"set": [1, 2, 3, 4, 1]}"#;
/// let res: Result<Doc, _> = serde_json::from_str(s);
/// assert!(res.is_err());
/// ```
pub mod sets_duplicate_value_is_error {
    use super::*;
    use crate::duplicate_key_impls::PreventDuplicateInsertsSet;

    /// Deserialize a set and return an error on duplicate values
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: PreventDuplicateInsertsSet<V>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct SeqVisitor<T, V> {
            marker: PhantomData<T>,
            set_item_type: PhantomData<V>,
        }

        impl<'de, T, V> Visitor<'de> for SeqVisitor<T, V>
        where
            T: PreventDuplicateInsertsSet<V>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a sequence")
            }

            #[inline]
            fn visit_seq<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some(value) = access.next_element()? {
                    if !values.insert(value) {
                        return Err(Error::custom("invalid entry: found duplicate value"));
                    };
                }

                Ok(values)
            }
        }

        let visitor = SeqVisitor {
            marker: PhantomData,
            set_item_type: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }

    /// Serialize the set with the default serializer
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// Ensure no duplicate keys exist in a map.
///
/// By default serde has a last-value-wins implementation, if duplicate keys for a map exist.
/// Sometimes it is desirable to know when such an event happens, as the first value is overwritten
/// and it can indicate an error in the serialized data.
///
/// This helper returns an error if two identical keys exist in a map.
///
/// The implementation supports both the [`HashMap`] and the [`BTreeMap`] from the standard library.
///
/// [`HashMap`]: std::collections::HashMap
/// [`BTreeMap`]: std::collections::HashMap
///
/// # Example
///
/// ```rust
/// # use serde::Deserialize;
/// # use std::collections::HashMap;
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize)]
/// struct Doc {
///     #[serde(with = "::serde_with::rust::maps_duplicate_key_is_error")]
///     map: HashMap<usize, usize>,
/// }
///
/// // Maps are serialized normally,
/// let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
/// let mut v = Doc {
///     map: HashMap::new(),
/// };
/// v.map.insert(1, 1);
/// v.map.insert(2, 2);
/// v.map.insert(3, 3);
/// assert_eq!(v, serde_json::from_str(s).unwrap());
///
/// // but create an error if duplicate keys, like the `1`, exist.
/// let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
/// let res: Result<Doc, _> = serde_json::from_str(s);
/// assert!(res.is_err());
/// ```
pub mod maps_duplicate_key_is_error {
    use super::*;
    use crate::duplicate_key_impls::PreventDuplicateInsertsMap;

    /// Deserialize a map and return an error on duplicate keys
    pub fn deserialize<'de, D, T, K, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: PreventDuplicateInsertsMap<K, V>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct MapVisitor<T, K, V> {
            marker: PhantomData<T>,
            map_key_type: PhantomData<K>,
            map_value_type: PhantomData<V>,
        }

        impl<'de, T, K, V> Visitor<'de> for MapVisitor<T, K, V>
        where
            T: PreventDuplicateInsertsMap<K, V>,
            K: Deserialize<'de>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a map")
            }

            #[inline]
            fn visit_map<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: MapAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some((key, value)) = access.next_entry()? {
                    if !values.insert(key, value) {
                        return Err(Error::custom("invalid entry: found duplicate key"));
                    };
                }

                Ok(values)
            }
        }

        let visitor = MapVisitor {
            marker: PhantomData,
            map_key_type: PhantomData,
            map_value_type: PhantomData,
        };
        deserializer.deserialize_map(visitor)
    }

    /// Serialize the map with the default serializer
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// *DEPRECATED* Ensure that the first value is taken, if duplicate values exist
///
/// This module implements the default behavior in serde.
#[deprecated = "This module does nothing. Remove the attribute. Serde's default behavior is to use the first value when deserializing a set."]
#[allow(deprecated)]
pub mod sets_first_value_wins {
    use super::*;
    use crate::duplicate_key_impls::DuplicateInsertsFirstWinsSet;

    /// Deserialize a set and keep the first of equal values
    #[deprecated = "This function does nothing. Remove the attribute. Serde's default behavior is to use the first value when deserializing a set."]
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: DuplicateInsertsFirstWinsSet<V>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct SeqVisitor<T, V> {
            marker: PhantomData<T>,
            set_item_type: PhantomData<V>,
        }

        impl<'de, T, V> Visitor<'de> for SeqVisitor<T, V>
        where
            T: DuplicateInsertsFirstWinsSet<V>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a sequence")
            }

            #[inline]
            fn visit_seq<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some(value) = access.next_element()? {
                    values.insert(value);
                }

                Ok(values)
            }
        }

        let visitor = SeqVisitor {
            marker: PhantomData,
            set_item_type: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }

    /// Serialize the set with the default serializer
    #[deprecated = "This function does nothing. Remove the attribute. Serde's default behavior is to use the first value when deserializing a set."]
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// Ensure that the last value is taken, if duplicate values exist
///
/// By default serde has a first-value-wins implementation, if duplicate keys for a set exist.
/// Sometimes the opposite strategy is desired. This helper implements a first-value-wins strategy.
///
/// The implementation supports both the [`HashSet`] and the [`BTreeSet`] from the standard library.
///
/// [`HashSet`]: std::collections::HashSet
/// [`BTreeSet`]: std::collections::HashSet
pub mod sets_last_value_wins {
    use super::*;
    use crate::duplicate_key_impls::DuplicateInsertsLastWinsSet;

    /// Deserialize a set and keep the last of equal values
    pub fn deserialize<'de, D, T, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: DuplicateInsertsLastWinsSet<V>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct SeqVisitor<T, V> {
            marker: PhantomData<T>,
            set_item_type: PhantomData<V>,
        }

        impl<'de, T, V> Visitor<'de> for SeqVisitor<T, V>
        where
            T: DuplicateInsertsLastWinsSet<V>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a sequence")
            }

            #[inline]
            fn visit_seq<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some(value) = access.next_element()? {
                    values.replace(value);
                }

                Ok(values)
            }
        }

        let visitor = SeqVisitor {
            marker: PhantomData,
            set_item_type: PhantomData,
        };
        deserializer.deserialize_seq(visitor)
    }

    /// Serialize the set with the default serializer
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// Ensure that the first key is taken, if duplicate keys exist
///
/// By default serde has a last-key-wins implementation, if duplicate keys for a map exist.
/// Sometimes the opposite strategy is desired. This helper implements a first-key-wins strategy.
///
/// The implementation supports both the [`HashMap`] and the [`BTreeMap`] from the standard library.
///
/// [`HashMap`]: std::collections::HashMap
/// [`BTreeMap`]: std::collections::HashMap
///
/// # Example
///
/// ```rust
/// # use serde::Deserialize;
/// # use std::collections::HashMap;
/// #
/// # #[derive(Debug, Eq, PartialEq)]
/// #[derive(Deserialize)]
/// struct Doc {
///     #[serde(with = "::serde_with::rust::maps_first_key_wins")]
///     map: HashMap<usize, usize>,
/// }
///
/// // Maps are serialized normally,
/// let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
/// let mut v = Doc {
///     map: HashMap::new(),
/// };
/// v.map.insert(1, 1);
/// v.map.insert(2, 2);
/// v.map.insert(3, 3);
/// assert_eq!(v, serde_json::from_str(s).unwrap());
///
/// // but create an error if duplicate keys, like the `1`, exist.
/// let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
/// let mut v = Doc {
///     map: HashMap::new(),
/// };
/// v.map.insert(1, 1);
/// v.map.insert(2, 2);
/// assert_eq!(v, serde_json::from_str(s).unwrap());
/// ```
pub mod maps_first_key_wins {
    use super::*;
    use crate::duplicate_key_impls::DuplicateInsertsFirstWinsMap;

    /// Deserialize a map and return an error on duplicate keys
    pub fn deserialize<'de, D, T, K, V>(deserializer: D) -> Result<T, D::Error>
    where
        T: DuplicateInsertsFirstWinsMap<K, V>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        struct MapVisitor<T, K, V> {
            marker: PhantomData<T>,
            map_key_type: PhantomData<K>,
            map_value_type: PhantomData<V>,
        }

        impl<'de, T, K, V> Visitor<'de> for MapVisitor<T, K, V>
        where
            T: DuplicateInsertsFirstWinsMap<K, V>,
            K: Deserialize<'de>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a map")
            }

            #[inline]
            fn visit_map<A>(self, mut access: A) -> Result<Self::Value, A::Error>
            where
                A: MapAccess<'de>,
            {
                let mut values = Self::Value::new(access.size_hint());

                while let Some((key, value)) = access.next_entry()? {
                    values.insert(key, value);
                }

                Ok(values)
            }
        }

        let visitor = MapVisitor {
            marker: PhantomData,
            map_key_type: PhantomData,
            map_value_type: PhantomData,
        };
        deserializer.deserialize_map(visitor)
    }

    /// Serialize the map with the default serializer
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// De/Serialize a [`Option<String>`] type while transforming the empty string to [`None`]
///
/// Convert an [`Option<T>`] from/to string using [`FromStr`] and [`AsRef<str>`] implementations.
/// An empty string is deserialized as [`None`] and a [`None`] vice versa.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed via [`NoneAsEmptyString`] and using the [`serde_as`] macro.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, NoneAsEmptyString};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct A {
///     #[serde_as(as = "NoneAsEmptyString")]
///     value: Option<String>,
/// }
/// # }
/// ```
///
/// # Examples
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use serde_with::rust::string_empty_as_none;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "string_empty_as_none")]
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
/// ```
///
/// [`NoneAsEmptyString`]: crate::NoneAsEmptyString
/// [`serde_as`]: crate::guide::serde_as
pub mod string_empty_as_none {
    use super::*;

    /// Deserialize an `Option<T>` from a string using `FromStr`
    pub fn deserialize<'de, D, S>(deserializer: D) -> Result<Option<S>, D::Error>
    where
        D: Deserializer<'de>,
        S: FromStr,
        S::Err: Display,
    {
        struct OptionStringEmptyNone<S>(PhantomData<S>);
        impl<'de, S> Visitor<'de> for OptionStringEmptyNone<S>
        where
            S: FromStr,
            S::Err: Display,
        {
            type Value = Option<S>;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("any string")
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match value {
                    "" => Ok(None),
                    v => S::from_str(v).map(Some).map_err(Error::custom),
                }
            }

            fn visit_string<E>(self, value: String) -> Result<Self::Value, E>
            where
                E: Error,
            {
                match &*value {
                    "" => Ok(None),
                    v => S::from_str(v).map(Some).map_err(Error::custom),
                }
            }

            // handles the `null` case
            fn visit_unit<E>(self) -> Result<Self::Value, E>
            where
                E: Error,
            {
                Ok(None)
            }
        }

        deserializer.deserialize_any(OptionStringEmptyNone(PhantomData))
    }

    /// Serialize a string from `Option<T>` using `AsRef<str>` or using the empty string if `None`.
    pub fn serialize<T, S>(option: &Option<T>, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: AsRef<str>,
        S: Serializer,
    {
        if let Some(value) = option {
            value.as_ref().serialize(serializer)
        } else {
            "".serialize(serializer)
        }
    }
}

/// De/Serialize a Map into a list of tuples
///
/// Some formats, like JSON, have limitations on the type of keys for maps.
/// In case of JSON, keys are restricted to strings.
/// Rust features more powerful keys, for example tuple, which can not be serialized to JSON.
///
/// This helper serializes the Map into a list of tuples, which does not have the same type restrictions.
/// The module can be applied on any type implementing `IntoIterator<Item = (&'a K, &'a V)>` and `FromIterator<(K, V)>`, with `K` and `V` being the key and value types.
/// From the standard library both [`HashMap`] and [`BTreeMap`] fullfil the condition and can be used here.
///
/// ## Converting to `serde_as`
///
/// If the map is of type [`HashMap`] or [`BTreeMap`] the same functionality can be expressed more clearly using the [`serde_as`] macro.
/// The `_` is a placeholder which works for any type which implements [`Serialize`]/[`Deserialize`], such as the tuple and `u32` type.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::serde_as;
/// # use std::collections::{BTreeMap, HashMap};
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "Vec<(_, _)>")]
///     hashmap: HashMap<(String, u32), u32>,
///     #[serde_as(as = "Vec<(_, _)>")]
///     btreemap: BTreeMap<(String, u32), u32>,
/// }
/// # }
/// ```
///
/// # Examples
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use std::collections::BTreeMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::map_as_tuple_list")]
///     s: BTreeMap<(String, u32), u32>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [["Hello", 123], 0],
///         [["World", 456], 1]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(1, v.s[&("World".to_string(), 456)]);
/// ```
///
/// The helper is generic over the hasher type of the [`HashMap`] and works with different variants, such as `FnvHashMap`.
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// #
/// use fnv::FnvHashMap;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::map_as_tuple_list")]
///     s: FnvHashMap<u32, bool>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [0, false],
///         [1, true]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(true, v.s[&1]);
/// ```
///
/// [`serde_as`]: crate::guide::serde_as
pub mod map_as_tuple_list {
    // Trait bounds based on this answer: https://stackoverflow.com/a/66600486/15470286
    use super::*;

    /// Serialize the map as a list of tuples
    pub fn serialize<'a, T, K, V, S>(map: T, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
        T: IntoIterator<Item = (&'a K, &'a V)>,
        T::IntoIter: ExactSizeIterator,
        K: Serialize + 'a,
        V: Serialize + 'a,
    {
        serializer.collect_seq(map)
    }

    /// Deserialize a map from a list of tuples
    pub fn deserialize<'de, T, K, V, D>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: FromIterator<(K, V)>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
    {
        struct SeqVisitor<T, K, V>(PhantomData<(T, K, V)>);

        impl<'de, T, K, V> Visitor<'de> for SeqVisitor<T, K, V>
        where
            T: FromIterator<(K, V)>,
            K: Deserialize<'de>,
            V: Deserialize<'de>,
        {
            type Value = T;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a list of key-value pairs")
            }

            fn visit_seq<A>(self, seq: A) -> Result<Self::Value, A::Error>
            where
                A: SeqAccess<'de>,
            {
                utils::SeqIter::new(seq).collect()
            }
        }

        deserializer.deserialize_seq(SeqVisitor(PhantomData))
    }
}

/// DEPRECATED De/Serialize a [`HashMap`] into a list of tuples
///
/// Use the [`map_as_tuple_list`] module which is more general than this.
/// It should work with everything convertible to and from an `Iterator` including [`BTreeMap`] and [`HashMap`].
///
/// ---
///
/// Some formats, like JSON, have limitations on the type of keys for maps.
/// In case of JSON, keys are restricted to strings.
/// Rust features more powerful keys, for example tuple, which can not be serialized to JSON.
///
/// This helper serializes the [`HashMap`] into a list of tuples, which does not have the same type restrictions.
///
/// If you need to de/serialize a [`BTreeMap`] then use [`btreemap_as_tuple_list`].
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed using the [`serde_as`] macro.
/// The `_` is a placeholder which works for any type which implements [`Serialize`]/[`Deserialize`], such as the tuple and `u32` type.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::serde_as;
/// # use std::collections::HashMap;
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "Vec<(_, _)>")]
///     s: HashMap<(String, u32), u32>,
/// }
/// # }
/// ```
///
/// # Examples
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use std::collections::HashMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::hashmap_as_tuple_list")]
///     s: HashMap<(String, u32), u32>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [["Hello", 123], 0],
///         [["World", 456], 1]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(1, v.s[&("World".to_string(), 456)]);
/// ```
///
/// The helper is generic over the hasher type of the [`HashMap`] and works with different variants, such as `FnvHashMap`.
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// #
/// use fnv::FnvHashMap;
///
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::hashmap_as_tuple_list")]
///     s: FnvHashMap<u32, bool>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [0, false],
///         [1, true]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(true, v.s[&1]);
/// ```
///
/// [`serde_as`]: crate::guide::serde_as
#[deprecated(
    since = "1.8.0",
    note = "Use the more general map_as_tuple_list module."
)]
pub mod hashmap_as_tuple_list {
    #[doc(inline)]
    #[deprecated(
        since = "1.8.0",
        note = "Use the more general map_as_tuple_list::deserialize function."
    )]
    pub use super::map_as_tuple_list::deserialize;
    #[doc(inline)]
    #[deprecated(
        since = "1.8.0",
        note = "Use the more general map_as_tuple_list::serialize function."
    )]
    pub use super::map_as_tuple_list::serialize;
}

/// DEPRECATED De/Serialize a [`BTreeMap`] into a list of tuples
///
/// Use the [`map_as_tuple_list`] module which is more general than this.
/// It should work with everything convertible to and from an `Iterator` including [`BTreeMap`] and [`HashMap`].
///
/// ---
///
/// Some formats, like JSON, have limitations on the type of keys for maps.
/// In case of JSON, keys are restricted to strings.
/// Rust features more powerful keys, for example tuple, which can not be serialized to JSON.
///
/// This helper serializes the [`BTreeMap`] into a list of tuples, which does not have the same type restrictions.
///
/// If you need to de/serialize a [`HashMap`] then use [`hashmap_as_tuple_list`].
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed using the [`serde_as`] macro.
/// The `_` is a placeholder which works for any type which implements [`Serialize`]/[`Deserialize`], such as the tuple and `u32` type.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::serde_as;
/// # use std::collections::BTreeMap;
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde_as(as = "Vec<(_, _)>")]
///     s: BTreeMap<(String, u32), u32>,
/// }
/// # }
/// ```
///
/// # Examples
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// # use serde_json::json;
/// # use std::collections::BTreeMap;
/// #
/// #[derive(Deserialize, Serialize)]
/// struct A {
///     #[serde(with = "serde_with::rust::btreemap_as_tuple_list")]
///     s: BTreeMap<(String, u32), u32>,
/// }
///
/// let v: A = serde_json::from_value(json!({
///     "s": [
///         [["Hello", 123], 0],
///         [["World", 456], 1]
///     ]
/// })).unwrap();
///
/// assert_eq!(2, v.s.len());
/// assert_eq!(1, v.s[&("World".to_string(), 456)]);
/// ```
///
/// [`serde_as`]: crate::guide::serde_as
#[deprecated(
    since = "1.8.0",
    note = "Use the more general map_as_tuple_list module."
)]
pub mod btreemap_as_tuple_list {
    #[doc(inline)]
    #[deprecated(
        since = "1.8.0",
        note = "Use the more general map_as_tuple_list::deserialize function."
    )]
    pub use super::map_as_tuple_list::deserialize;
    #[doc(inline)]
    #[deprecated(
        since = "1.8.0",
        note = "Use the more general map_as_tuple_list::serialize function."
    )]
    pub use super::map_as_tuple_list::serialize;
}

/// This serializes a list of tuples into a map and back
///
/// Normally, you want to use a [`HashMap`] or a [`BTreeMap`] when deserializing a map.
/// However, sometimes this is not possible due to type contains, e.g., if the type implements neither [`Hash`] nor [`Ord`].
/// Another use case is deserializing a map with duplicate keys.
///
/// The implementation is generic using the [`FromIterator`] and [`IntoIterator`] traits.
/// Therefore, all of [`Vec`], [`VecDeque`](std::collections::VecDeque), and [`LinkedList`](std::collections::LinkedList) and anything which implements those are supported.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed using the [`serde_as`] macro.
/// The `_` is a placeholder which works for any type which implements [`Serialize`]/[`Deserialize`], such as the tuple and `u32` type.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::{Deserialize, Serialize};
/// # use serde_with::serde_as;
/// # use std::collections::BTreeMap;
/// #
/// #[serde_as]
/// #[derive(Deserialize, Serialize)]
/// struct S {
///     #[serde_as(as = "BTreeMap<_, _>")] // HashMap will also work
///     s: Vec<(i32, String)>,
/// }
/// # }
/// ```
///
/// # Examples
///
/// `Wrapper` does not implement [`Hash`] nor [`Ord`], thus prohibiting the use [`HashMap`] or [`BTreeMap`].
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// #
/// #[derive(Debug, Deserialize, Serialize, Default)]
/// struct S {
///     #[serde(with = "serde_with::rust::tuple_list_as_map")]
///     s: Vec<(Wrapper<i32>, Wrapper<String>)>,
/// }
///
/// #[derive(Clone, Debug, Serialize, Deserialize)]
/// #[serde(transparent)]
/// struct Wrapper<T>(T);
///
/// let from = r#"{
///   "s": {
///     "1": "Hi",
///     "2": "Cake",
///     "99": "Lie"
///   }
/// }"#;
/// let mut expected = S::default();
/// expected.s.push((Wrapper(1), Wrapper("Hi".into())));
/// expected.s.push((Wrapper(2), Wrapper("Cake".into())));
/// expected.s.push((Wrapper(99), Wrapper("Lie".into())));
///
/// let res: S = serde_json::from_str(from).unwrap();
/// for ((exp_k, exp_v), (res_k, res_v)) in expected.s.iter().zip(&res.s) {
///     assert_eq!(exp_k.0, res_k.0);
///     assert_eq!(exp_v.0, res_v.0);
/// }
/// assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
/// ```
///
/// In this example, the serialized format contains duplicate keys, which is not supported with [`HashMap`] or [`BTreeMap`].
///
/// ```
/// # use serde::{Deserialize, Serialize};
/// #
/// #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
/// struct S {
///     #[serde(with = "serde_with::rust::tuple_list_as_map")]
///     s: Vec<(i32, String)>,
/// }
///
/// let from = r#"{
///   "s": {
///     "1": "Hi",
///     "1": "Cake",
///     "1": "Lie"
///   }
/// }"#;
/// let mut expected = S::default();
/// expected.s.push((1, "Hi".into()));
/// expected.s.push((1, "Cake".into()));
/// expected.s.push((1, "Lie".into()));
///
/// let res: S = serde_json::from_str(from).unwrap();
/// assert_eq!(3, res.s.len());
/// assert_eq!(expected, res);
/// assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
/// ```
///
/// [`serde_as`]: crate::guide::serde_as
pub mod tuple_list_as_map {
    use super::*;

    /// Serialize any iteration of tuples into a map.
    pub fn serialize<'a, I, K, V, S>(iter: I, serializer: S) -> Result<S::Ok, S::Error>
    where
        I: IntoIterator<Item = &'a (K, V)>,
        I::IntoIter: ExactSizeIterator,
        K: Serialize + 'a,
        V: Serialize + 'a,
        S: Serializer,
    {
        // Convert &(K, V) to (&K, &V) for collect_map.
        let iter = iter.into_iter().map(|(k, v)| (k, v));
        serializer.collect_map(iter)
    }

    /// Deserialize a map into an iterator of tuples.
    pub fn deserialize<'de, I, K, V, D>(deserializer: D) -> Result<I, D::Error>
    where
        I: FromIterator<(K, V)>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
        D: Deserializer<'de>,
    {
        deserializer.deserialize_map(MapVisitor(PhantomData))
    }

    #[allow(clippy::type_complexity)]
    struct MapVisitor<I, K, V>(PhantomData<fn() -> (I, K, V)>);

    impl<'de, I, K, V> Visitor<'de> for MapVisitor<I, K, V>
    where
        I: FromIterator<(K, V)>,
        K: Deserialize<'de>,
        V: Deserialize<'de>,
    {
        type Value = I;

        fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
            formatter.write_str("a map")
        }

        fn visit_map<A>(self, map: A) -> Result<Self::Value, A::Error>
        where
            A: MapAccess<'de>,
        {
            utils::MapIter::new(map).collect()
        }
    }
}

/// Deserialize from bytes or string
///
/// Any Rust [`String`] can be converted into bytes, i.e., `Vec<u8>`.
/// Accepting both as formats while deserializing can be helpful while interacting with language
/// which have a looser definition of string than Rust.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed via [`BytesOrString`] and using the [`serde_as`] macro.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, BytesOrString};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct A {
///     #[serde_as(as = "BytesOrString")]
///     bos: Vec<u8>,
/// }
/// # }
/// ```
///
/// # Example
/// ```rust
/// # use serde::{Deserialize, Serialize};
/// #
/// #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
/// struct S {
///     #[serde(deserialize_with = "serde_with::rust::bytes_or_string::deserialize")]
///     bos: Vec<u8>,
/// }
///
/// // Here we deserialize from a byte array ...
/// let from = r#"{
///   "bos": [
///     0,
///     1,
///     2,
///     3
///   ]
/// }"#;
/// let expected = S {
///     bos: vec![0, 1, 2, 3],
/// };
///
/// let res: S = serde_json::from_str(from).unwrap();
/// assert_eq!(expected, res);
///
/// // and serialization works too.
/// assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
///
/// // But we also support deserializing from a String
/// let from = r#"{
///   "bos": "✨Works!"
/// }"#;
/// let expected = S {
///     bos: "✨Works!".as_bytes().to_vec(),
/// };
///
/// let res: S = serde_json::from_str(from).unwrap();
/// assert_eq!(expected, res);
/// ```
///
/// [`BytesOrString`]: crate::BytesOrString
/// [`serde_as`]: crate::guide::serde_as
pub mod bytes_or_string {
    use super::*;

    /// Deserialize a [`Vec<u8>`] from either bytes or string
    pub fn deserialize<'de, D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(BytesOrStringVisitor)
    }

    struct BytesOrStringVisitor;

    impl<'de> Visitor<'de> for BytesOrStringVisitor {
        type Value = Vec<u8>;

        fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
            formatter.write_str("a list of bytes or a string")
        }

        fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E> {
            Ok(v.to_vec())
        }

        fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E> {
            Ok(v)
        }

        fn visit_str<E>(self, v: &str) -> Result<Self::Value, E> {
            Ok(v.as_bytes().to_vec())
        }

        fn visit_string<E>(self, v: String) -> Result<Self::Value, E> {
            Ok(v.into_bytes())
        }

        fn visit_seq<A>(self, seq: A) -> Result<Self::Value, A::Error>
        where
            A: SeqAccess<'de>,
        {
            utils::SeqIter::new(seq).collect()
        }
    }
}

/// Deserialize value and return [`Default`] on error
///
/// The main use case is ignoring error while deserializing.
/// Instead of erroring, it simply deserializes the [`Default`] variant of the type.
/// It is not possible to find the error location, i.e., which field had a deserialization error, with this method.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed via [`DefaultOnError`] and using the [`serde_as`] macro.
/// It can be combined with other converts as shown.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, DefaultOnError, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct A {
///     #[serde_as(as = "DefaultOnError")]
///     value: u32,
///     #[serde_as(as = "DefaultOnError<DisplayFromStr>")]
///     value2: u32,
/// }
/// # }
/// ```
///
/// [`DefaultOnError`]: crate::DefaultOnError
/// [`serde_as`]: crate::guide::serde_as
///
/// # Examples
///
/// ```
/// # use serde::Deserialize;
/// #
/// #[derive(Deserialize)]
/// struct A {
///     #[serde(deserialize_with = "serde_with::rust::default_on_error::deserialize")]
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
/// let a: A = serde_json::from_str(r#"{"value": {}}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // Missing entries still cause errors
/// assert!(serde_json::from_str::<A>(r#"{  }"#).is_err());
/// ```
///
/// Deserializing missing values can be supported by adding the `default` field attribute:
///
/// ```
/// # use serde::Deserialize;
/// #
/// #[derive(Deserialize)]
/// struct B {
///     #[serde(default, deserialize_with = "serde_with::rust::default_on_error::deserialize")]
///     value: u32,
/// }
///
/// let b: B = serde_json::from_str(r#"{  }"#).unwrap();
/// assert_eq!(0, b.value);
/// ```
pub mod default_on_error {
    use super::*;

    /// Deserialize T and return the [`Default`] value on error
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: Deserialize<'de> + Default,
    {
        #[derive(Debug, serde::Deserialize)]
        #[serde(untagged)]
        enum GoodOrError<T> {
            Good(T),
            // This consumes one "item" when `T` errors while deserializing.
            // This is necessary to make this work, when instead of having a direct value
            // like integer or string, the deserializer sees a list or map.
            Error(serde::de::IgnoredAny),
        }

        Ok(match Deserialize::deserialize(deserializer) {
            Ok(GoodOrError::Good(res)) => res,
            _ => Default::default(),
        })
    }

    /// Serialize value with the default serializer
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// Deserialize default value if encountering `null`.
///
/// One use case are JSON APIs in which the `null` value represents some default state.
/// This adapter allows to turn the `null` directly into the [`Default`] value of the type.
///
/// ## Converting to `serde_as`
///
/// The same functionality can be more clearly expressed via [`DefaultOnNull`] and using the [`serde_as`] macro.
/// It can be combined with other convertes as shown.
///
/// ```rust
/// # #[cfg(feature = "macros")] {
/// # use serde::Deserialize;
/// # use serde_with::{serde_as, DefaultOnNull, DisplayFromStr};
/// #
/// #[serde_as]
/// #[derive(Deserialize)]
/// struct A {
///     #[serde_as(as = "DefaultOnNull")]
///     value: u32,
///     #[serde_as(as = "DefaultOnNull<DisplayFromStr>")]
///     value2: u32,
/// }
/// # }
/// ```
///
/// [`DefaultOnNull`]: crate::DefaultOnNull
/// [`serde_as`]: crate::guide::serde_as
///
/// # Examples
///
/// ```
/// # use serde::Deserialize;
/// #
/// #[derive(Deserialize)]
/// struct A {
///     #[serde(deserialize_with = "serde_with::rust::default_on_null::deserialize")]
///     value: u32,
/// }
///
/// let a: A = serde_json::from_str(r#"{"value": 123}"#).unwrap();
/// assert_eq!(123, a.value);
///
/// let a: A = serde_json::from_str(r#"{"value": null}"#).unwrap();
/// assert_eq!(0, a.value);
///
/// // String is invalid type
/// assert!(serde_json::from_str::<A>(r#"{"value": "123"}"#).is_err());
/// ```
pub mod default_on_null {
    use super::*;

    /// Deserialize T and return the [`Default`] value if original value is `null`
    pub fn deserialize<'de, D, T>(deserializer: D) -> Result<T, D::Error>
    where
        D: Deserializer<'de>,
        T: Deserialize<'de> + Default,
    {
        Ok(Option::deserialize(deserializer)?.unwrap_or_default())
    }

    /// Serialize value with the default serializer
    pub fn serialize<T, S>(value: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        value.serialize(serializer)
    }
}

/// Deserialize any value, ignore it, and return the default value for the type being deserialized.
///
/// This function can be used in two different ways:
///
/// 1. It is useful for instance to create an enum with a catch-all variant that will accept any incoming data.
/// 2. [`untagged`] enum representations do not allow the `other` annotation as the fallback enum variant.
///     With this function you can emulate an `other` variant, which can deserialize any data carrying enum.
///
/// **Note:** Using this function will prevent deserializing data-less enum variants.
/// If this is a problem depends on the data format.
/// For example, deserializing `"Bar"` as an enum in JSON would fail, since it carries no data.
///
/// # Examples
///
/// ## Deserializing a heterogeneous collection of XML nodes
///
/// When [`serde-xml-rs`] deserializes an XML tag to an enum, it always maps the tag
/// name to the enum variant name, and the tag attributes and children to the enum contents.
/// This means that in order for an enum variant to accept any XML tag, it both has to use
/// `#[serde(other)]` to accept any tag name, and `#[serde(deserialize_with = "deserialize_ignore_any")]`
/// to accept any attributes and children.
///
/// ```rust
/// # use serde::Deserialize;
/// use serde_with::rust::deserialize_ignore_any;
///
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize)]
/// #[serde(rename_all = "lowercase")]
/// enum Item {
///     Foo(String),
///     Bar(String),
///     #[serde(other, deserialize_with = "deserialize_ignore_any")]
///     Other,
/// }
///
/// // Deserialize this XML
/// # let items: Vec<Item> = serde_xml_rs::from_str(
/// r"
/// <foo>a</foo>
/// <bar>b</bar>
/// <foo>c</foo>
/// <unknown>d</unknown>
/// "
/// # ).unwrap();
///
/// // into these Items
/// # let expected =
/// vec![
///     Item::Foo(String::from("a")),
///     Item::Bar(String::from("b")),
///     Item::Foo(String::from("c")),
///     Item::Other,
/// ]
/// # ;
/// # assert_eq!(expected, items);
/// ```
///
/// ## Simulating an `other` enum variant in an `untagged` enum
///
/// ```rust
/// # use serde::Deserialize;
/// # use serde_json::json;
/// use serde_with::rust::deserialize_ignore_any;
///
/// # #[derive(Debug, PartialEq)]
/// #[derive(Deserialize)]
/// #[serde(untagged)]
/// enum Item {
///     Foo{x: u8},
///     #[serde(deserialize_with = "deserialize_ignore_any")]
///     Other,
/// }
///
/// // Deserialize this JSON
/// # let items: Vec<Item> = serde_json::from_value(
/// json!([
///     {"y": 1},
///     {"x": 1},
/// ])
/// # ).unwrap();
///
/// // into these Items
/// # let expected =
/// vec![Item::Other, Item::Foo{x: 1}]
/// # ;
/// # assert_eq!(expected, items);
/// ```
///
/// [`serde-xml-rs`]: https://docs.rs/serde-xml-rs
/// [`untagged`]: https://serde.rs/enum-representations.html#untagged
pub fn deserialize_ignore_any<'de, D: Deserializer<'de>, T: Default>(
    deserializer: D,
) -> Result<T, D::Error> {
    serde::de::IgnoredAny::deserialize(deserializer).map(|_| T::default())
}
