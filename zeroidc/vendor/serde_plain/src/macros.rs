#[macro_export]
/// Implements [`FromStr`](std::str::FromStr) for a type that forwards to [`Deserialize`](serde::Deserialize).
///
/// ```rust
/// # #[macro_use] extern crate serde_derive;
/// use serde::Deserialize;
/// use serde_plain::derive_fromstr_from_deserialize;
/// # fn main() {
///
/// #[derive(Deserialize, Debug)]
/// pub enum MyEnum {
///     VariantA,
///     VariantB,
/// }
///
/// derive_fromstr_from_deserialize!(MyEnum);
/// # }
/// ```
///
/// This automatically implements [`FromStr`](std::str::FromStr) which will
/// invoke the [`from_str`](crate::from_str) method from this crate.
///
/// Additionally this macro supports a second argument which can be the
/// error type to use.  In that case `From<serde_plain::Error>` needs
/// to be implemented for that error.
///
/// A third form with a conversion function as second argument is supported.
/// The closure needs to be in the form `|err| -> ErrType { ... }`:
///
/// ```rust
/// # #[macro_use] extern crate serde_derive;
/// use serde::Deserialize;
/// use serde_plain::derive_fromstr_from_deserialize;
/// # fn main() {
///
/// #[derive(Deserialize, Debug)]
/// pub enum MyEnum {
///     VariantA,
///     VariantB,
/// }
///
/// #[derive(Debug)]
/// pub struct MyError(String);
///
/// derive_fromstr_from_deserialize!(MyEnum, |err| -> MyError { MyError(err.to_string()) });
/// # }
/// ```
macro_rules! derive_fromstr_from_deserialize {
    ($type:ty) => {
        impl ::std::str::FromStr for $type {
            type Err = $crate::Error;
            fn from_str(s: &str) -> ::std::result::Result<$type, Self::Err> {
                $crate::from_str(s)
            }
        }
    };
    ($type:ty, |$var:ident| -> $err_type:ty { $err_conv:expr }) => {
        impl ::std::str::FromStr for $type {
            type Err = $err_type;
            fn from_str(s: &str) -> ::std::result::Result<$type, Self::Err> {
                $crate::from_str(s).map_err(|$var| ($err_conv))
            }
        }
    };
    ($type:ty, $err_type:ty) => {
        impl ::std::str::FromStr for $type {
            type Err = $err_type;
            fn from_str(s: &str) -> ::std::result::Result<$type, Self::Err> {
                $crate::from_str(s).map_err(|e| e.into())
            }
        }
    };
}

/// Legacy alias for [`derive_fromstr_from_deserialize`].
#[deprecated(note = "legacy alias for derive_fromstr_from_deserialize")]
#[doc(hidden)]
#[macro_export]
macro_rules! forward_from_str_to_serde {
    ($($tt:tt)*) => { $crate::derive_fromstr_from_deserialize!($($tt)*); }
}

#[macro_export]
/// Implements [`Display`](std::fmt::Display) for a type that forwards to [`Serialize`](serde::Serialize).
///
/// ```rust
/// # #[macro_use] extern crate serde_derive;
/// use serde::Deserialize;
/// use serde_plain::derive_display_from_serialize;
/// # fn main() {
///
/// #[derive(Serialize, Debug)]
/// pub enum MyEnum {
///     VariantA,
///     VariantB,
/// }
///
/// derive_display_from_serialize!(MyEnum);
/// # }
/// ```
///
/// This automatically implements [`Display`](std::fmt::Display) which will
/// invoke the [`to_string`](crate::to_string) method from this crate. In case
/// that fails the method will panic.
macro_rules! derive_display_from_serialize {
    ($type:ident $(:: $type_extra:ident)* < $($lt:lifetime),+ >) => {
        impl<$($lt,)*> ::std::fmt::Display for $type$(:: $type_extra)*<$($lt,)*> {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                write!(f, "{}", $crate::to_string(self).unwrap())
            }
        }
    };
    ($type:ty) => {
        impl ::std::fmt::Display for $type {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                write!(f, "{}", $crate::to_string(self).unwrap())
            }
        }
    };
}

/// Legacy alias for [`derive_fromstr_from_serialize`].
#[deprecated(note = "legacy alias for derive_display_from_serialize")]
#[doc(hidden)]
#[macro_export]
macro_rules! forward_display_to_serde {
    ($($tt:tt)*) => { $crate::derive_display_from_serialize!($($tt)*); }
}

/// Derives [`Deserialize`](serde::Serialize) for a type that implements [`FromStr`](std::str::FromStr).
///
/// ```rust
/// use std::str::FromStr;
/// use std::num::ParseIntError;
/// use serde_plain::derive_deserialize_from_fromstr;
/// # fn main() {
///
/// pub struct MyStruct(u32);
///
/// impl FromStr for MyStruct {
///     type Err = ParseIntError;
///     fn from_str(value: &str) -> Result<MyStruct, Self::Err> {
///         Ok(MyStruct(value.parse()?))
///     }
/// }
///
/// derive_deserialize_from_fromstr!(MyStruct, "valid positive number");
/// # }
/// ```
///
/// This automatically implements [`Serialize`](serde::Serialize) which will
/// invoke the [`from_str`](crate::from_str) function on the target type
/// internally. First argument is the name of the type, the second is a message
/// for the expectation error (human readable type effectively).
#[macro_export]
macro_rules! derive_deserialize_from_fromstr {
    ($type:ty, $expectation:expr) => {
        impl<'de> ::serde::de::Deserialize<'de> for $type {
            fn deserialize<D>(deserializer: D) -> ::std::result::Result<Self, D::Error>
            where
                D: ::serde::de::Deserializer<'de>,
            {
                struct V;

                impl<'de> ::serde::de::Visitor<'de> for V {
                    type Value = $type;

                    fn expecting(
                        &self,
                        formatter: &mut ::std::fmt::Formatter,
                    ) -> ::std::fmt::Result {
                        formatter.write_str($expectation)
                    }

                    fn visit_str<E>(self, value: &str) -> ::std::result::Result<$type, E>
                    where
                        E: ::serde::de::Error,
                    {
                        value.parse().map_err(|_| {
                            ::serde::de::Error::invalid_value(
                                ::serde::de::Unexpected::Str(value),
                                &self,
                            )
                        })
                    }
                }

                deserializer.deserialize_str(V)
            }
        }
    };
}

/// Legacy alias for [`derive_fromstr_from_deserialize`].
#[deprecated(note = "legacy alias for derive_deserialize_from_fromstr")]
#[doc(hidden)]
#[macro_export]
macro_rules! derive_deserialize_from_str {
    ($($tt:tt)*) => { $crate::derive_deserialize_from_fromstr!($($tt)*); }
}

/// Derives [`Serialize`](serde::Serialize) a type that implements [`Display`](std::fmt::Display).
///
/// ```rust
/// use std::fmt;
/// use serde_plain::derive_serialize_from_display;
/// # fn main() {
///
/// pub struct MyStruct(u32);
///
/// impl fmt::Display for MyStruct {
///     fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
///         write!(f, "{}", self.0)
///     }
/// }
///
/// derive_serialize_from_display!(MyStruct);
/// # }
/// ```
///
/// This automatically implements [`Serialize`](serde::Serialize) which will
/// invoke the [`to_string`](crate::to_string) method on the target.
#[macro_export]
macro_rules! derive_serialize_from_display {
    ($type:ident $(:: $type_extra:ident)* < $($lt:lifetime),+ >) => {
        impl<$($lt,)*> ::serde::ser::Serialize for $type$(:: $type_extra)*<$($lt,)*> {
            fn serialize<S>(&self, serializer: S) -> ::std::result::Result<S::Ok, S::Error>
            where
                S: ::serde::ser::Serializer,
            {
                serializer.serialize_str(&self.to_string())
            }
        }
    };
    ($type:ty) => {
        impl ::serde::ser::Serialize for $type {
            fn serialize<S>(&self, serializer: S) -> ::std::result::Result<S::Ok, S::Error>
            where
                S: ::serde::ser::Serializer,
            {
                serializer.serialize_str(&self.to_string())
            }
        }
    };
}

#[test]
fn test_derive_display_from_serialize_lifetimes() {
    use serde_derive::Serialize;

    #[derive(Serialize)]
    struct MyType<'a>(&'a str);

    mod inner {
        use serde_derive::Serialize;

        #[derive(Serialize)]
        pub struct MyType<'a>(pub &'a str);
    }

    derive_display_from_serialize!(MyType<'a>);
    derive_display_from_serialize!(inner::MyType<'a>);

    assert_eq!(MyType("x").to_string(), "x");
    assert_eq!(inner::MyType("x").to_string(), "x");
}

#[test]
fn test_derive_serialize_from_display_lifetimes() {
    use serde_derive::Deserialize;

    #[derive(Deserialize)]
    struct MyType<'a>(&'a str);

    impl<'a> std::fmt::Display for MyType<'a> {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            write!(f, "{}", self.0)
        }
    }

    mod inner {
        use serde_derive::Deserialize;

        #[derive(Deserialize)]
        pub struct MyType<'a>(pub &'a str);

        impl<'a> std::fmt::Display for MyType<'a> {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                write!(f, "{}", self.0)
            }
        }
    }

    derive_serialize_from_display!(MyType<'a>);
    derive_serialize_from_display!(inner::MyType<'a>);

    assert_eq!(crate::to_string(&MyType("x")).unwrap(), "x");
    assert_eq!(crate::to_string(&inner::MyType("x")).unwrap(), "x");
}
