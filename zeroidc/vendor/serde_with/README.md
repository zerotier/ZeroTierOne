# Custom de/serialization functions for Rust's [serde](https://serde.rs)

[![crates.io badge](https://img.shields.io/crates/v/serde_with.svg)](https://crates.io/crates/serde_with/)
[![Build Status](https://github.com/jonasbb/serde_with/workflows/Rust%20CI/badge.svg)](https://github.com/jonasbb/serde_with)
[![codecov](https://codecov.io/gh/jonasbb/serde_with/branch/master/graph/badge.svg)](https://codecov.io/gh/jonasbb/serde_with)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/4322/badge)](https://bestpractices.coreinfrastructure.org/projects/4322)
[![Binder](https://img.shields.io/badge/Try%20on%20-binder-579ACA.svg?logo=data:image/png;base64,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)](https://mybinder.org/v2/gist/jonasbb/18b9aece4c17f617b1c2b3946d29eeb0/HEAD?filepath=serde-with-demo.ipynb)

---

This crate provides custom de/serialization helpers to use in combination with [serde's with-annotation][with-annotation] and with the improved [`serde_as`][as-annotation]-annotation.
Some common use cases are:

* De/Serializing a type using the `Display` and `FromStr` traits, e.g., for `u8`, `url::Url`, or `mime::Mime`.
     Check [`DisplayFromStr`][] or [`serde_with::rust::display_fromstr`][display_fromstr] for details.
* Support for arrays larger than 32 elements or using const generics.
    With `serde_as` large arrays are supported, even if they are nested in other types.
    `[bool; 64]`, `Option<[u8; M]>`, and `Box<[[u8; 64]; N]>` are all supported, as [this examples shows](#large-and-const-generic-arrays).
* Skip serializing all empty `Option` types with [`#[skip_serializing_none]`][skip_serializing_none].
* Apply a prefix to each field name of a struct, without changing the de/serialize implementations of the struct using [`with_prefix!`][].
* Deserialize a comma separated list like `#hash,#tags,#are,#great` into a `Vec<String>`.
     Check the documentation for [`serde_with::rust::StringWithSeparator::<CommaSeparator>`][StringWithSeparator].

### Getting Help

**Check out the [user guide][user guide] to find out more tips and tricks about this crate.**

For further help using this crate you can [open a new discussion](https://github.com/jonasbb/serde_with/discussions/new) or ask on [users.rust-lang.org](https://users.rust-lang.org/).
For bugs, please open a [new issue](https://github.com/jonasbb/serde_with/issues/new) on GitHub.

## Use `serde_with` in your Project

Add this to your `Cargo.toml`:

```toml
[dependencies.serde_with]
version = "1.14.0"
features = [ "..." ]
```

The crate contains different features for integration with other common crates.
Check the [feature flags][] section for information about all available features.

## Examples

Annotate your struct or enum to enable the custom de/serializer.
The `#[serde_as]` attribute must be place *before* the `#[derive]`.

### `DisplayFromStr`

```rust
#[serde_as]
#[derive(Deserialize, Serialize)]
struct Foo {
    // Serialize with Display, deserialize with FromStr
    #[serde_as(as = "DisplayFromStr")]
    bar: u8,
}

// This will serialize
Foo {bar: 12}

// into this JSON
{"bar": "12"}
```

### Large and const-generic arrays

serde does not support arrays with more than 32 elements or using const-generics.
The `serde_as` attribute allows to circumvent this restriction, even for nested types and nested arrays.

```rust
#[serde_as]
#[derive(Deserialize, Serialize)]
struct Arrays<const N: usize, const M: usize> {
    #[serde_as(as = "[_; N]")]
    constgeneric: [bool; N],

    #[serde_as(as = "Box<[[_; 64]; N]>")]
    nested: Box<[[u8; 64]; N]>,

    #[serde_as(as = "Option<[_; M]>")]
    optional: Option<[u8; M]>,
}

// This allows us to serialize a struct like this
let arrays: Arrays<100, 128> = Arrays {
    constgeneric: [true; 100],
    nested: Box::new([[111; 64]; 100]),
    optional: Some([222; 128])
};
assert!(serde_json::to_string(&arrays).is_ok());
```

### `skip_serializing_none`

This situation often occurs with JSON, but other formats also support optional fields.
If many fields are optional, putting the annotations on the structs can become tedious.
The `#[skip_serializing_none]` attribute must be place *before* the `#[derive]`.

```rust
#[skip_serializing_none]
#[derive(Deserialize, Serialize)]
struct Foo {
    a: Option<usize>,
    b: Option<usize>,
    c: Option<usize>,
    d: Option<usize>,
    e: Option<usize>,
    f: Option<usize>,
    g: Option<usize>,
}

// This will serialize
Foo {a: None, b: None, c: None, d: Some(4), e: None, f: None, g: Some(7)}

// into this JSON
{"d": 4, "g": 7}
```

### Advanced `serde_as` usage

This example is mainly supposed to highlight the flexibility of the `serde_as`-annotation compared to [serde's with-annotation][with-annotation].
More details about `serde_as` can be found in the [user guide][].

```rust
#[serde_as]
#[derive(Deserialize, Serialize)]
struct Foo {
     // Serialize them into a list of number as seconds
     #[serde_as(as = "Vec<DurationSeconds>")]
     durations: Vec<Duration>,
     // We can treat a Vec like a map with duplicates.
     // JSON only allows string keys, so convert i32 to strings
     // The bytes will be hex encoded
     #[serde_as(as = "BTreeMap<DisplayFromStr, Hex>")]
     bytes: Vec<(i32, Vec<u8>)>,
}

// This will serialize
Foo {
    durations: vec![Duration::new(5, 0), Duration::new(3600, 0), Duration::new(0, 0)],
    bytes: vec![
        (1, vec![0, 1, 2]),
        (-100, vec![100, 200, 255]),
        (1, vec![0, 111, 222]),
    ],
}

// into this JSON
{
    "durations": [5, 3600, 0],
    "bytes": {
        "1": "000102",
        "-100": "64c8ff",
        "1": "006fde"
    }
}
```

[`DisplayFromStr`]: https://docs.rs/serde_with/1.14.0/serde_with/struct.DisplayFromStr.html
[`with_prefix!`]: https://docs.rs/serde_with/1.14.0/serde_with/macro.with_prefix.html
[display_fromstr]: https://docs.rs/serde_with/1.14.0/serde_with/rust/display_fromstr/index.html
[feature flags]: https://docs.rs/serde_with/1.14.0/serde_with/guide/feature_flags/index.html
[skip_serializing_none]: https://docs.rs/serde_with/1.14.0/serde_with/attr.skip_serializing_none.html
[StringWithSeparator]: https://docs.rs/serde_with/1.14.0/serde_with/rust/struct.StringWithSeparator.html
[user guide]: https://docs.rs/serde_with/1.14.0/serde_with/guide/index.html
[with-annotation]: https://serde.rs/field-attrs.html#with
[as-annotation]: https://docs.rs/serde_with/1.14.0/serde_with/guide/serde_as/index.html

## License

Licensed under either of

* Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contribution

For detailed contribution instructions please read [`CONTRIBUTING.md`].

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.

[`CONTRIBUTING.md`]: https://github.com/jonasbb/serde_with/blob/master/CONTRIBUTING.md
