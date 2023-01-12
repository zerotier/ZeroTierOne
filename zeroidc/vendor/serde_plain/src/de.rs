use serde::de::{self, Deserialize, IntoDeserializer, Visitor};

use crate::error::Error;

/// A simple deserializer that works with plain strings.
pub struct Deserializer<'de> {
    input: &'de str,
}

impl<'de> Deserializer<'de> {
    pub fn new(input: &'de str) -> Self {
        Deserializer { input }
    }
}

/// Deserialize an instance of type `T` from a string of plain text.
///
/// This deserializes the string into an object with the `Deserializer`
/// and returns it.  This requires that the type is a simple one
/// (integer, string etc.).
pub fn from_str<'a, T>(s: &'a str) -> Result<T, Error>
where
    T: Deserialize<'a>,
{
    T::deserialize(Deserializer::new(s))
}

macro_rules! forward_to_deserialize_from_str {
    ($func:ident, $visit_func:ident, $tymsg:expr) => {
        fn $func<V>(self, visitor: V) -> Result<V::Value, Error>
        where
            V: Visitor<'de>,
        {
            visitor.$visit_func(
                self.input
                    .parse()
                    .map_err(|e| Error::Parse($tymsg, format!("{}", e)))?,
            )
        }
    };
}

impl<'de> de::Deserializer<'de> for Deserializer<'de> {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    forward_to_deserialize_from_str!(deserialize_bool, visit_bool, "boolean");
    forward_to_deserialize_from_str!(deserialize_i8, visit_i8, "i8");
    forward_to_deserialize_from_str!(deserialize_i16, visit_i16, "i16");
    forward_to_deserialize_from_str!(deserialize_i32, visit_i32, "i32");
    forward_to_deserialize_from_str!(deserialize_i64, visit_i64, "i64");
    forward_to_deserialize_from_str!(deserialize_u8, visit_u8, "u8");
    forward_to_deserialize_from_str!(deserialize_u16, visit_u16, "u16");
    forward_to_deserialize_from_str!(deserialize_u32, visit_u32, "u32");
    forward_to_deserialize_from_str!(deserialize_u64, visit_u64, "u64");
    forward_to_deserialize_from_str!(deserialize_f32, visit_f32, "f32");
    forward_to_deserialize_from_str!(deserialize_f64, visit_f64, "f64");
    forward_to_deserialize_from_str!(deserialize_char, visit_char, "char");

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        visitor.visit_borrowed_str(self.input)
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_bytes<V>(self, _visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("bytes"))
    }

    fn deserialize_byte_buf<V>(self, _visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("bytes"))
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        if self.input.is_empty() {
            visitor.visit_none()
        } else {
            visitor.visit_some(self)
        }
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        if self.input.is_empty() {
            visitor.visit_unit()
        } else {
            Err(Error::Message("expected empty string for unit".into()))
        }
    }

    fn deserialize_unit_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        self.deserialize_unit(visitor)
    }

    fn deserialize_newtype_struct<V>(
        self,
        _name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_seq<V>(self, _visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("seq"))
    }

    fn deserialize_tuple<V>(self, _len: usize, _visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("tuple"))
    }

    fn deserialize_tuple_struct<V>(
        self,
        _name: &'static str,
        _len: usize,
        _visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("tuple struct"))
    }

    fn deserialize_map<V>(self, _visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("map"))
    }

    fn deserialize_struct<V>(
        self,
        _name: &'static str,
        _fields: &'static [&'static str],
        _visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        Err(Error::ImpossibleDeserialization("struct"))
    }

    fn deserialize_enum<V>(
        self,
        _name: &'static str,
        _variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        visitor.visit_enum(self.input.into_deserializer())
    }

    fn deserialize_identifier<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_ignored_any<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: Visitor<'de>,
    {
        self.deserialize_any(visitor)
    }
}
