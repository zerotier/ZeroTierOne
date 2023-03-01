# `serde_with` User Guide

This crate provides helper functions to extend and change how [`serde`] serializes different data types.
For example, you can serialize [a map as a sequence of tuples][crate::guide::serde_as#maps-to-vec-of-tuples], serialize [using the `Display` and `FromStr` traits][`DisplayFromStr`], or serialize [an empty `String` like `None`][NoneAsEmptyString].
`serde_with` covers types from the Rust Standard Library and some common crates like [`chrono`][serde_with_chrono].

[**A list of all supported transformations is available on this page.**](crate::guide::serde_as_transformations)

The crate offers four types of functionality.

## 1. A more flexible and composable replacement for the with annotation, called `serde_as` *(v1.5.0+)*

This is an alternative to [serde's with-annotation][with-annotation], which adds flexibility and composability to the scheme.
The main downside is that it work with fewer types than [with-annotations][with-annotation].
However, all types from the Rust Standard Library should be supported in all combinations and any missing entry is a bug.

The `serde_as` scheme is based on two new traits: [`SerializeAs`] and [`DeserializeAs`].  
[Check out the detailed page about `serde_as` and the available features.](crate::guide::serde_as)

### Example

```rust
# use serde::{Deserialize, Serialize};
# use serde_with::{serde_as, DisplayFromStr};
# use std::collections::HashMap;
# use std::net::Ipv4Addr;
#
#[serde_as]
# #[derive(Debug, PartialEq, Eq)]
#[derive(Deserialize, Serialize)]
struct Data {
    // Type does not implement Serialize or Deserialize
    #[serde_as(as = "DisplayFromStr")]
    address: Ipv4Addr,
    // Treat the Vec like a map with duplicates
    // Convert u32 into a String and keep the String the same type
    #[serde_as(as = "HashMap<DisplayFromStr, _>")]
    vec_as_map: Vec<(u32, String)>,
}

let data = Data {
    address: Ipv4Addr::new(192, 168, 0, 1),
    vec_as_map: vec![
        (123, "Hello".into()),
        (456, "World".into()),
        (123, "Hello".into()),
    ],
};

let json = r#"{
  "address": "192.168.0.1",
  "vec_as_map": {
    "123": "Hello",
    "456": "World",
    "123": "Hello"
  }
}"#;

// Test Serialization
assert_eq!(json, serde_json::to_string_pretty(&data).unwrap());
// Test Deserialization
assert_eq!(data, serde_json::from_str(json).unwrap());
```

## 2. Integration with serde's with-annotation

[serde's with-annotation][with-annotation] allows specifying a different serialization or deserialization function for a field.
It is useful to adapt the serialization of existing types to the requirements of a protocol.
Most modules in this crate can be used together with the with-annotation.

The annotation approach has one big drawback, in that it is very inflexible.
It allows specifying arbitrary serialization code, but the code has to perform the correct transformations.
It is not possible to combine multiple of those functions.
One common use case for this is the serialization of collections like `Vec`.
If you have a field of type `T`, you can apply the with-annotation, but if you have a field of type `Vec<T>`, there is no way to re-use the same functions for the with-annotation.
This inflexibility is fixed in the `serde_as` scheme presented above.

The example shows a similar setup as in the `serde_as` example above, but using the with-annotation.

### Example

```rust
# use serde::{Deserialize, Serialize};
# use std::net::Ipv4Addr;
#
# #[derive(Debug, PartialEq, Eq)]
#[derive(Deserialize, Serialize)]
struct Data {
    // Type does not implement Serialize or Deserialize
    #[serde(with = "serde_with::rust::display_fromstr")]
    address: Ipv4Addr,
    // Treat the Vec like a map with duplicates
    #[serde(with = "serde_with::rust::tuple_list_as_map")]
    vec_as_map: Vec<(String, u32)>,
}

let data = Data {
    address: Ipv4Addr::new(192, 168, 0, 1),
    vec_as_map: vec![
        ("Hello".into(), 123),
        ("World".into(), 456),
        ("Hello".into(), 123),
    ],
};

let json = r#"{
  "address": "192.168.0.1",
  "vec_as_map": {
    "Hello": 123,
    "World": 456,
    "Hello": 123
  }
}"#;

// Test Serialization
assert_eq!(json, serde_json::to_string_pretty(&data).unwrap());
// Test Deserialization
assert_eq!(data, serde_json::from_str(json).unwrap());
```

## 3. proc-macros to make it easier to use both above parts

The proc-macros are an optional addition and improve the user experience for common tasks.
We have already seen how the `serde_as` attribute is used to define the serialization instructions.

The proc-macro attributes are defined in the [`serde_with_macros`] crate and re-exported from the root of this crate.
The proc-macros are optional, but enabled by default.
For further details, please refer to the documentation of each proc-macro.

## 4. Derive macros to implement `Deserialize` and `Serialize`

The derive macros work similar to the serde provided ones, but they do implement other de/serialization schemes.
For example, the derives [`DeserializeFromStr`] and [`SerializeDisplay`] require that the type also implement [`FromStr`] and [`Display`] and de/serializes from/to a string instead of the usual way of iterating over all fields.

## Migrating from the with-annotations to `serde_as`

Each old style module explains how it can be converted to `serde_as`.
Not all modules have such a description since not all are migrated and some are hard to implement in the `serde_as` system.

[`Display`]: std::fmt::Display
[`FromStr`]: std::str::FromStr
[`serde_with_macros`]: serde_with_macros
[serde_with_chrono]: crate::chrono
[with-annotation]: https://serde.rs/field-attrs.html#with
