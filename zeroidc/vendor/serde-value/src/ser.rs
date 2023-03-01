use serde::ser;
use std::collections::BTreeMap;
use std::error::Error;
use std::fmt;

use crate::Value;

#[derive(Debug)]
pub enum SerializerError {
    Custom(String),
}

impl fmt::Display for SerializerError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SerializerError::Custom(ref s) => fmt.write_str(s),
        }
    }
}

impl Error for SerializerError {
    fn description(&self) -> &str {
        "Value serializer error"
    }
}

impl ser::Error for SerializerError {
    fn custom<T: fmt::Display>(msg: T) -> SerializerError {
        SerializerError::Custom(msg.to_string())
    }
}

impl ser::Serialize for Value {
    fn serialize<S: ser::Serializer>(&self, s: S) -> Result<S::Ok, S::Error> {
        match *self {
            Value::Bool(v) => s.serialize_bool(v),
            Value::U8(v) => s.serialize_u8(v),
            Value::U16(v) => s.serialize_u16(v),
            Value::U32(v) => s.serialize_u32(v),
            Value::U64(v) => s.serialize_u64(v),
            Value::I8(v) => s.serialize_i8(v),
            Value::I16(v) => s.serialize_i16(v),
            Value::I32(v) => s.serialize_i32(v),
            Value::I64(v) => s.serialize_i64(v),
            Value::F32(v) => s.serialize_f32(v),
            Value::F64(v) => s.serialize_f64(v),
            Value::Char(v) => s.serialize_char(v),
            Value::String(ref v) => s.serialize_str(v),
            Value::Unit => s.serialize_unit(),
            Value::Option(None) => s.serialize_none(),
            Value::Option(Some(ref v)) => s.serialize_some(v),
            Value::Newtype(ref v) => s.serialize_newtype_struct("", v),
            Value::Seq(ref v) => v.serialize(s),
            Value::Map(ref v) => v.serialize(s),
            Value::Bytes(ref v) => s.serialize_bytes(v),
        }
    }
}

pub fn to_value<T: ser::Serialize>(value: T) -> Result<Value, SerializerError> {
    value.serialize(Serializer)
}

struct Serializer;

impl ser::Serializer for Serializer {
    type Ok = Value;
    type Error = SerializerError;
    type SerializeSeq = SerializeSeq;
    type SerializeTuple = SerializeTuple;
    type SerializeTupleStruct = SerializeTupleStruct;
    type SerializeTupleVariant = SerializeTupleVariant;
    type SerializeMap = SerializeMap;
    type SerializeStruct = SerializeStruct;
    type SerializeStructVariant = SerializeStructVariant;

    fn serialize_bool(self, v: bool) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Bool(v))
    }

    fn serialize_i8(self, v: i8) -> Result<Self::Ok, Self::Error> {
        Ok(Value::I8(v))
    }

    fn serialize_i16(self, v: i16) -> Result<Self::Ok, Self::Error> {
        Ok(Value::I16(v))
    }

    fn serialize_i32(self, v: i32) -> Result<Self::Ok, Self::Error> {
        Ok(Value::I32(v))
    }

    fn serialize_i64(self, v: i64) -> Result<Self::Ok, Self::Error> {
        Ok(Value::I64(v))
    }

    fn serialize_u8(self, v: u8) -> Result<Self::Ok, Self::Error> {
        Ok(Value::U8(v))
    }

    fn serialize_u16(self, v: u16) -> Result<Self::Ok, Self::Error> {
        Ok(Value::U16(v))
    }

    fn serialize_u32(self, v: u32) -> Result<Self::Ok, Self::Error> {
        Ok(Value::U32(v))
    }

    fn serialize_u64(self, v: u64) -> Result<Self::Ok, Self::Error> {
        Ok(Value::U64(v))
    }

    fn serialize_f32(self, v: f32) -> Result<Self::Ok, Self::Error> {
        Ok(Value::F32(v))
    }

    fn serialize_f64(self, v: f64) -> Result<Self::Ok, Self::Error> {
        Ok(Value::F64(v))
    }

    fn serialize_char(self, v: char) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Char(v))
    }

    fn serialize_str(self, v: &str) -> Result<Self::Ok, Self::Error> {
        Ok(Value::String(v.to_string()))
    }

    fn serialize_bytes(self, v: &[u8]) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Bytes(v.to_vec()))
    }

    fn serialize_none(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Option(None))
    }

    fn serialize_some<T: ?Sized>(
        self,
        value: &T
    ) -> Result<Self::Ok, Self::Error>
    where
        T: ser::Serialize
    {
        value.serialize(Serializer).map(|v| Value::Option(Some(Box::new(v))))
    }

    fn serialize_unit(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Unit)
    }

    fn serialize_unit_struct(
        self,
        _name: &'static str
    ) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Unit)
    }

    fn serialize_unit_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
    ) -> Result<Self::Ok, Self::Error> {
        Ok(Value::String(variant.to_string()))
    }

    fn serialize_newtype_struct<T: ?Sized>(
        self,
        _name: &'static str,
        value: &T
    ) -> Result<Self::Ok, Self::Error>
    where
        T: ser::Serialize
    {
        value.serialize(Serializer).map(|v| Value::Newtype(Box::new(v)))
    }

    fn serialize_newtype_variant<T: ?Sized>(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        value: &T
    ) -> Result<Self::Ok, Self::Error>
    where
        T: ser::Serialize
    {
        value.serialize(Serializer).map(|v| {
            let mut map = BTreeMap::new();
            map.insert(Value::String(variant.to_string()), v);
            Value::Map(map)
        })
    }

    fn serialize_seq(
        self,
        _len: Option<usize>
    ) -> Result<Self::SerializeSeq, Self::Error> {
        Ok(SerializeSeq(vec![]))
    }

    fn serialize_tuple(
        self,
        _len: usize
    ) -> Result<Self::SerializeTuple, Self::Error> {
        Ok(SerializeTuple(vec![]))
    }

    fn serialize_tuple_struct(
        self,
        _name: &'static str,
        _len: usize
    ) -> Result<Self::SerializeTupleStruct, Self::Error> {
        Ok(SerializeTupleStruct(vec![]))
    }

    fn serialize_tuple_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        len: usize
    ) -> Result<Self::SerializeTupleVariant, Self::Error> {
        Ok(SerializeTupleVariant(
            Value::String(variant.to_string()),
            Vec::with_capacity(len),
        ))
    }

    fn serialize_map(
        self,
        _len: Option<usize>
    ) -> Result<Self::SerializeMap, Self::Error> {
        Ok(SerializeMap { map: BTreeMap::new(), key: None })
    }

    fn serialize_struct(
        self,
        _name: &'static str,
        _len: usize
    ) -> Result<Self::SerializeStruct, Self::Error> {
        Ok(SerializeStruct(BTreeMap::new()))
    }

    fn serialize_struct_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        _len: usize
    ) -> Result<Self::SerializeStructVariant, Self::Error> {
        Ok(SerializeStructVariant(
            Value::String(variant.to_string()),
            BTreeMap::new(),
        ))
    }
}

struct SerializeSeq(Vec<Value>);

impl ser::SerializeSeq for SerializeSeq {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_element<T: ?Sized>(
        &mut self,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let value = value.serialize(Serializer)?;
        self.0.push(value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Seq(self.0))
    }
}

struct SerializeTuple(Vec<Value>);

impl ser::SerializeTuple for SerializeTuple {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_element<T: ?Sized>(
        &mut self,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let value = value.serialize(Serializer)?;
        self.0.push(value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Seq(self.0))
    }
}

struct SerializeTupleStruct(Vec<Value>);

impl ser::SerializeTupleStruct for SerializeTupleStruct {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_field<T: ?Sized>(
        &mut self,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let value = value.serialize(Serializer)?;
        self.0.push(value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Seq(self.0))
    }
}

struct SerializeTupleVariant(Value, Vec<Value>);

impl ser::SerializeTupleVariant for SerializeTupleVariant {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_field<T: ?Sized>(
        &mut self,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let value = value.serialize(Serializer)?;
        self.1.push(value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        let mut map = BTreeMap::new();
        map.insert(self.0, Value::Seq(self.1));
        Ok(Value::Map(map))
    }
}

struct SerializeMap {
    map: BTreeMap<Value, Value>,
    key: Option<Value>,
}

impl ser::SerializeMap for SerializeMap {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_key<T: ?Sized>(&mut self, key: &T) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let key = key.serialize(Serializer)?;
        self.key = Some(key);
        Ok(())
    }

    fn serialize_value<T: ?Sized>(
        &mut self,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let value = value.serialize(Serializer)?;
        self.map.insert(self.key.take().unwrap(), value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Map(self.map))
    }
}

struct SerializeStruct(BTreeMap<Value, Value>);

impl ser::SerializeStruct for SerializeStruct {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_field<T: ?Sized>(
        &mut self,
        key: &'static str,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let key = Value::String(key.to_string());
        let value = value.serialize(Serializer)?;
        self.0.insert(key, value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(Value::Map(self.0))
    }
}

struct SerializeStructVariant(Value, BTreeMap<Value, Value>);

impl ser::SerializeStructVariant for SerializeStructVariant {
    type Ok = Value;
    type Error = SerializerError;

    fn serialize_field<T: ?Sized>(
        &mut self,
        key: &'static str,
        value: &T
    ) -> Result<(), Self::Error>
    where
        T: ser::Serialize
    {
        let key = Value::String(key.to_string());
        let value = value.serialize(Serializer)?;
        self.1.insert(key, value);
        Ok(())
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        let mut map = BTreeMap::new();
        map.insert(self.0, Value::Map(self.1));
        Ok(Value::Map(map))
    }
}
