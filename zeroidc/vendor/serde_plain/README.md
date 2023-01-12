# Serde Plain

This crate implements a plain text serializer and deserializer. It can only
serialize and deserialize primitives and derivatives thereof (like basic enums
or newtypes). It internally uses the `FromStr` and `Display` trait to convert
objects around.

## From String

To parse a value from a string the from_str helper can be used:

```rust
assert_eq!(serde_plain::from_str::<i32>("42").unwrap(), 42);
```

This is particularly useful if enums are in use:

```rust
use serde::Deserialize;

#[derive(Deserialize, Debug, PartialEq, Eq)]
pub enum MyEnum {
    VariantA,
    VariantB,
}

assert_eq!(serde_plain::from_str::<MyEnum>("VariantA").unwrap(), MyEnum::VariantA);
```

## To String

The inverse is also possible with to_string:

```rust
assert_eq!(serde_plain::to_string(&true).unwrap(), "true");
```