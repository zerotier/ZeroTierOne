use serde::{forward_to_deserialize_any, de};
use std::collections::BTreeMap;
use std::error::Error;
use std::fmt;
use std::marker::PhantomData;

use crate::Value;

#[derive(Debug)]
pub enum Unexpected {
    Bool(bool),
    Unsigned(u64),
    Signed(i64),
    Float(f64),
    Char(char),
    Str(String),
    Bytes(Vec<u8>),
    Unit,
    Option,
    NewtypeStruct,
    Seq,
    Map,
    Enum,
    UnitVariant,
    NewtypeVariant,
    TupleVariant,
    StructVariant,
    Other(String),
}

impl<'a> From<de::Unexpected<'a>> for Unexpected {
    fn from(unexp: de::Unexpected) -> Unexpected {
        match unexp {
            de::Unexpected::Bool(v) => Unexpected::Bool(v),
            de::Unexpected::Unsigned(v) => Unexpected::Unsigned(v),
            de::Unexpected::Signed(v) => Unexpected::Signed(v),
            de::Unexpected::Float(v) => Unexpected::Float(v),
            de::Unexpected::Char(v) => Unexpected::Char(v),
            de::Unexpected::Str(v) => Unexpected::Str(v.to_owned()),
            de::Unexpected::Bytes(v) => Unexpected::Bytes(v.to_owned()),
            de::Unexpected::Unit => Unexpected::Unit,
            de::Unexpected::Option => Unexpected::Option,
            de::Unexpected::NewtypeStruct => Unexpected::NewtypeStruct,
            de::Unexpected::Seq => Unexpected::Seq,
            de::Unexpected::Map => Unexpected::Map,
            de::Unexpected::Enum => Unexpected::Enum,
            de::Unexpected::UnitVariant => Unexpected::UnitVariant,
            de::Unexpected::NewtypeVariant => Unexpected::NewtypeVariant,
            de::Unexpected::TupleVariant => Unexpected::TupleVariant,
            de::Unexpected::StructVariant => Unexpected::StructVariant,
            de::Unexpected::Other(v) => Unexpected::Other(v.to_owned()),
        }
    }
}

impl Unexpected {
    pub fn to_unexpected<'a>(&'a self) -> de::Unexpected<'a> {
        match *self {
            Unexpected::Bool(v) => de::Unexpected::Bool(v),
            Unexpected::Unsigned(v) => de::Unexpected::Unsigned(v),
            Unexpected::Signed(v) => de::Unexpected::Signed(v),
            Unexpected::Float(v) => de::Unexpected::Float(v),
            Unexpected::Char(v) => de::Unexpected::Char(v),
            Unexpected::Str(ref v) => de::Unexpected::Str(v),
            Unexpected::Bytes(ref v) => de::Unexpected::Bytes(v),
            Unexpected::Unit => de::Unexpected::Unit,
            Unexpected::Option => de::Unexpected::Option,
            Unexpected::NewtypeStruct => de::Unexpected::NewtypeStruct,
            Unexpected::Seq => de::Unexpected::Seq,
            Unexpected::Map => de::Unexpected::Map,
            Unexpected::Enum => de::Unexpected::Enum,
            Unexpected::UnitVariant => de::Unexpected::UnitVariant,
            Unexpected::NewtypeVariant => de::Unexpected::NewtypeVariant,
            Unexpected::TupleVariant => de::Unexpected::TupleVariant,
            Unexpected::StructVariant => de::Unexpected::StructVariant,
            Unexpected::Other(ref v) => de::Unexpected::Other(v),
        }
    }

}

#[derive(Debug)]
pub enum DeserializerError {
    Custom(String),
    InvalidType(Unexpected, String),
    InvalidValue(Unexpected, String),
    InvalidLength(usize, String),
    UnknownVariant(String, &'static [&'static str]),
    UnknownField(String, &'static [&'static str]),
    MissingField(&'static str),
    DuplicateField(&'static str),
}

impl de::Error for DeserializerError {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        DeserializerError::Custom(msg.to_string())
    }

    fn invalid_type(unexp: de::Unexpected, exp: &dyn de::Expected) -> Self {
        DeserializerError::InvalidType(unexp.into(), exp.to_string())
    }

    fn invalid_value(unexp: de::Unexpected, exp: &dyn de::Expected) -> Self {
        DeserializerError::InvalidValue(unexp.into(), exp.to_string())
    }

    fn invalid_length(len: usize, exp: &dyn de::Expected) -> Self {
        DeserializerError::InvalidLength(len, exp.to_string())
    }

    fn unknown_variant(field: &str, expected: &'static [&'static str]) -> Self {
        DeserializerError::UnknownVariant(field.into(), expected)
    }

    fn unknown_field(field: &str, expected: &'static [&'static str]) -> Self {
        DeserializerError::UnknownField(field.into(), expected)
    }

    fn missing_field(field: &'static str) -> Self {
        DeserializerError::MissingField(field)
    }

    fn duplicate_field(field: &'static str) -> Self {
        DeserializerError::DuplicateField(field)
    }
}

impl DeserializerError {
    pub fn to_error<E: de::Error>(&self) -> E {
        match *self {
            DeserializerError::Custom(ref msg) => E::custom(msg.clone()),
            DeserializerError::InvalidType(ref unexp, ref exp) => {
                E::invalid_type(unexp.to_unexpected(), &&**exp)
            }
            DeserializerError::InvalidValue(ref unexp, ref exp) => {
                E::invalid_value(unexp.to_unexpected(), &&**exp)
            }
            DeserializerError::InvalidLength(len, ref exp) => E::invalid_length(len, &&**exp),
            DeserializerError::UnknownVariant(ref field, exp) => E::unknown_variant(field, exp),
            DeserializerError::UnknownField(ref field, exp) => E::unknown_field(field, exp),
            DeserializerError::MissingField(field) => E::missing_field(field),
            DeserializerError::DuplicateField(field) => E::missing_field(field),
        }
    }

    pub fn into_error<E: de::Error>(self) -> E {
        self.to_error()
    }
}

impl Error for DeserializerError {
    fn description(&self) -> &str {
        "Value deserializer error"
    }
}

impl fmt::Display for DeserializerError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            DeserializerError::Custom(ref msg) => write!(f, "{}", msg),
            DeserializerError::InvalidType(ref unexp, ref exp) => {
                write!(f, "Invalid type {}. Expected {}", unexp.to_unexpected(), exp)
            }
            DeserializerError::InvalidValue(ref unexp, ref exp) => {
                write!(f, "Invalid value {}. Expected {}", unexp.to_unexpected(), exp)
            }
            DeserializerError::InvalidLength(len, ref exp) => {
                write!(f, "Invalid length {}. Expected {}", len, exp)
            }
            DeserializerError::UnknownVariant(ref field, exp) => {
                write!(f, "Unknown variant {}. Expected one of {}", field, exp.join(", "))
            },
            DeserializerError::UnknownField(ref field, exp) => {
                write!(f, "Unknown field {}. Expected one of {}", field, exp.join(", "))
            }
            DeserializerError::MissingField(field) => write!(f, "Missing field {}", field),
            DeserializerError::DuplicateField(field) => write!(f, "Duplicate field {}", field),
        }
    }
}

impl From<de::value::Error> for DeserializerError {
    fn from(e: de::value::Error) -> DeserializerError {
        DeserializerError::Custom(e.to_string())
    }
}

pub struct ValueVisitor;

impl<'de> de::Visitor<'de> for ValueVisitor {
    type Value = Value;

    fn expecting(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("any value")
    }

    fn visit_bool<E>(self, value: bool) -> Result<Value, E> {
        Ok(Value::Bool(value))
    }

    fn visit_i8<E>(self, value: i8) -> Result<Value, E> {
        Ok(Value::I8(value))
    }

    fn visit_i16<E>(self, value: i16) -> Result<Value, E> {
        Ok(Value::I16(value))
    }

    fn visit_i32<E>(self, value: i32) -> Result<Value, E> {
        Ok(Value::I32(value))
    }

    fn visit_i64<E>(self, value: i64) -> Result<Value, E> {
        Ok(Value::I64(value))
    }

    fn visit_u8<E>(self, value: u8) -> Result<Value, E> {
        Ok(Value::U8(value))
    }

    fn visit_u16<E>(self, value: u16) -> Result<Value, E> {
        Ok(Value::U16(value))
    }

    fn visit_u32<E>(self, value: u32) -> Result<Value, E> {
        Ok(Value::U32(value))
    }

    fn visit_u64<E>(self, value: u64) -> Result<Value, E> {
        Ok(Value::U64(value))
    }

    fn visit_f32<E>(self, value: f32) -> Result<Value, E> {
        Ok(Value::F32(value))
    }

    fn visit_f64<E>(self, value: f64) -> Result<Value, E> {
        Ok(Value::F64(value))
    }

    fn visit_char<E>(self, value: char) -> Result<Value, E> {
        Ok(Value::Char(value))
    }

    fn visit_str<E>(self, value: &str) -> Result<Value, E> {
        Ok(Value::String(value.into()))
    }

    fn visit_string<E>(self, value: String) -> Result<Value, E> {
        Ok(Value::String(value))
    }

    fn visit_unit<E>(self) -> Result<Value, E> {
        Ok(Value::Unit)
    }

    fn visit_none<E>(self) -> Result<Value, E> {
        Ok(Value::Option(None))
    }

    fn visit_some<D: de::Deserializer<'de>>(self, d: D) -> Result<Value, D::Error> {
        d.deserialize_any(ValueVisitor).map(|v| Value::Option(Some(Box::new(v))))
    }

    fn visit_newtype_struct<D: de::Deserializer<'de>>(self, d: D) -> Result<Value, D::Error> {
        d.deserialize_any(ValueVisitor).map(|v| Value::Newtype(Box::new(v)))
    }

    fn visit_seq<V: de::SeqAccess<'de>>(self, mut visitor: V) -> Result<Value, V::Error> {
        let mut values = Vec::new();
        while let Some(elem) = visitor.next_element()? {
            values.push(elem);
        }
        Ok(Value::Seq(values))
    }

    fn visit_map<V: de::MapAccess<'de>>(self, mut visitor: V) -> Result<Value, V::Error> {
        let mut values = BTreeMap::new();
        while let Some((key, value)) = visitor.next_entry()? {
            values.insert(key, value);
        }
        Ok(Value::Map(values))
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Value, E> {
        Ok(Value::Bytes(v.into()))
    }

    fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Value, E> {
        Ok(Value::Bytes(v))
    }
}

impl<'de> de::Deserialize<'de> for Value {
    fn deserialize<D: de::Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        d.deserialize_any(ValueVisitor)
    }
}

impl<'de> de::IntoDeserializer<'de, DeserializerError> for Value {
    type Deserializer = Value;

    fn into_deserializer(self) -> Value {
        self
    }
}

pub struct ValueDeserializer<E> {
    value: Value,
    error: PhantomData<fn() -> E>,
}

impl<E> ValueDeserializer<E> {
    pub fn new(value: Value) -> Self {
        ValueDeserializer {
            value: value,
            error: Default::default(),
        }
    }

    pub fn into_value(self) -> Value {
        self.value
    }
}

impl<'de, E> de::Deserializer<'de> for ValueDeserializer<E> where E: de::Error {
    type Error = E;

    fn deserialize_any<V: de::Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        match self.value {
            Value::Bool(v) => visitor.visit_bool(v),
            Value::U8(v) => visitor.visit_u8(v),
            Value::U16(v) => visitor.visit_u16(v),
            Value::U32(v) => visitor.visit_u32(v),
            Value::U64(v) => visitor.visit_u64(v),
            Value::I8(v) => visitor.visit_i8(v),
            Value::I16(v) => visitor.visit_i16(v),
            Value::I32(v) => visitor.visit_i32(v),
            Value::I64(v) => visitor.visit_i64(v),
            Value::F32(v) => visitor.visit_f32(v),
            Value::F64(v) => visitor.visit_f64(v),
            Value::Char(v) => visitor.visit_char(v),
            Value::String(v) => visitor.visit_string(v),
            Value::Unit => visitor.visit_unit(),
            Value::Option(None) => visitor.visit_none(),
            Value::Option(Some(v)) => visitor.visit_some(ValueDeserializer::new(*v)),
            Value::Newtype(v) => visitor.visit_newtype_struct(ValueDeserializer::new(*v)),
            Value::Seq(v) => {
                visitor.visit_seq(de::value::SeqDeserializer::new(v.into_iter().map(ValueDeserializer::new)))
            },
            Value::Map(v) => {
                visitor.visit_map(de::value::MapDeserializer::new(v.into_iter().map(|(k, v)| (
                    ValueDeserializer::new(k),
                    ValueDeserializer::new(v),
                ))))
            },
            Value::Bytes(v) => visitor.visit_byte_buf(v),
        }
    }

    fn deserialize_option<V: de::Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        match self.value {
            Value::Option(..) => self.deserialize_any(visitor),
            Value::Unit => visitor.visit_unit(),
            _ => visitor.visit_some(self)
        }
    }

    fn deserialize_enum<V: de::Visitor<'de>>(self,
                                             _name: &'static str,
                                             _variants: &'static [&'static str],
                                             visitor: V)
                                             -> Result<V::Value, Self::Error> {
        let (variant, value) = match self.value {
            Value::Map(value) => {
                let mut iter = value.into_iter();
                let (variant, value) = match iter.next() {
                    Some(v) => v,
                    None => {
                        return Err(de::Error::invalid_value(de::Unexpected::Map,
                                                            &"map with a single key"));
                    }
                };
                // enums are encoded as maps with a single key:value pair
                if iter.next().is_some() {
                    return Err(de::Error::invalid_value(de::Unexpected::Map,
                                                        &"map with a single key"));
                }
                (variant, Some(value))
            }
            Value::String(variant) => (Value::String(variant), None),
            other => {
                return Err(de::Error::invalid_type(other.unexpected(), &"string or map"));
            }
        };

        let d = EnumDeserializer {
            variant: variant,
            value: value,
            error: Default::default(),
        };
        visitor.visit_enum(d)
    }

    fn deserialize_newtype_struct<V: de::Visitor<'de>>(self,
                                                       _name: &'static str,
                                                       visitor: V)
                                                       -> Result<V::Value, Self::Error> {
        match self.value {
            Value::Newtype(v) => visitor.visit_newtype_struct(ValueDeserializer::new(*v)),
            _ => visitor.visit_newtype_struct(self),
        }
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string unit
        seq bytes byte_buf map unit_struct
        tuple_struct struct tuple ignored_any identifier
    }
}

impl<'de, E> de::IntoDeserializer<'de, E> for ValueDeserializer<E> where E: de::Error {
    type Deserializer = Self;

    fn into_deserializer(self) -> Self::Deserializer {
        self
    }
}

impl<'de> de::Deserializer<'de> for Value {
    type Error = DeserializerError;

    fn deserialize_any<V: de::Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        ValueDeserializer::new(self).deserialize_any(visitor)
    }

    fn deserialize_option<V: de::Visitor<'de>>(self, visitor: V) -> Result<V::Value, Self::Error> {
        ValueDeserializer::new(self).deserialize_option(visitor)
    }

    fn deserialize_enum<V: de::Visitor<'de>>(self,
                                             name: &'static str,
                                             variants: &'static [&'static str],
                                             visitor: V)
                                             -> Result<V::Value, Self::Error> {
        ValueDeserializer::new(self).deserialize_enum(name, variants, visitor)
    }

    fn deserialize_newtype_struct<V: de::Visitor<'de>>(self,
                                                       name: &'static str,
                                                       visitor: V)
                                                       -> Result<V::Value, Self::Error> {
        ValueDeserializer::new(self).deserialize_newtype_struct(name, visitor)
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string unit
        seq bytes byte_buf map unit_struct
        tuple_struct struct tuple ignored_any identifier
    }
}

struct EnumDeserializer<E> {
    variant: Value,
    value: Option<Value>,
    error: PhantomData<fn() -> E>,
}

impl<'de, E> de::EnumAccess<'de> for EnumDeserializer<E> where E: de::Error {
    type Error = E;
    type Variant = VariantDeserializer<Self::Error>;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, VariantDeserializer<Self::Error>), Self::Error>
        where V: de::DeserializeSeed<'de>
    {
        let visitor = VariantDeserializer {
            value: self.value,
            error: Default::default(),
        };
        seed.deserialize(ValueDeserializer::new(self.variant)).map(|v| (v, visitor))
    }
}

struct VariantDeserializer<E> {
    value: Option<Value>,
    error: PhantomData<fn() -> E>,
}

impl<'de, E> de::VariantAccess<'de> for VariantDeserializer<E> where E: de::Error {
    type Error = E;

    fn unit_variant(self) -> Result<(), Self::Error> {
        match self.value {
            Some(value) => de::Deserialize::deserialize(ValueDeserializer::new(value)),
            None => Ok(()),
        }
    }

    fn newtype_variant_seed<T>(self, seed: T) -> Result<T::Value, Self::Error>
        where T: de::DeserializeSeed<'de>
    {
        match self.value {
            Some(value) => seed.deserialize(ValueDeserializer::new(value)),
            None => Err(de::Error::invalid_type(de::Unexpected::UnitVariant, &"newtype variant")),
        }
    }

    fn tuple_variant<V>(self, _len: usize, visitor: V) -> Result<V::Value, Self::Error>
        where V: de::Visitor<'de>
    {
        match self.value {
            Some(Value::Seq(v)) => {
                de::Deserializer::deserialize_any(
                    de::value::SeqDeserializer::new(v.into_iter().map(ValueDeserializer::new)),
                    visitor)
            }
            Some(other) => Err(de::Error::invalid_type(other.unexpected(), &"tuple variant")),
            None => Err(de::Error::invalid_type(de::Unexpected::UnitVariant, &"tuple variant")),
        }
    }

    fn struct_variant<V>(self,
                       _fields: &'static [&'static str],
                       visitor: V)
                       -> Result<V::Value, Self::Error>
        where V: de::Visitor<'de>
    {
        match self.value {
            Some(Value::Map(v)) => {
                de::Deserializer::deserialize_any(
                    de::value::MapDeserializer::new(v.into_iter().map(|(k, v)| (
                        ValueDeserializer::new(k),
                        ValueDeserializer::new(v),
                    ))),
                    visitor)
            }
            Some(other) => Err(de::Error::invalid_type(other.unexpected(), &"struct variant")),
            None => Err(de::Error::invalid_type(de::Unexpected::UnitVariant, &"struct variant")),
        }
    }
}
