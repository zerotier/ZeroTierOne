//! Buffer for serializing data.
//!
//! This is a copy and improvement of the `serde` private type:
//! <https://github.com/serde-rs/serde/blob/55a7cedd737278a9d75a2efd038c6f38b8c38bd6/serde/src/private/ser.rs#L338-L997>
//! The code is very stable in the `serde` crate, so no maintainability problem is expected.
//!
//! Since the type is private we copy the type here.
//! `serde` is licensed as MIT+Apache2, the same as this crate.
//!
//! This version carries improvements compared to `serde`'s version.
//! The types support 128-bit integers, which is supported for all targets in Rust 1.40+.
//! The [`ContentSerializer`] can also be configured to human readable or compact representation.

use alloc::{borrow::ToOwned, boxed::Box, string::String, vec::Vec};
use core::marker::PhantomData;
use serde::ser::{self, Serialize, Serializer};

#[derive(Debug)]
pub(crate) enum Content {
    Bool(bool),

    U8(u8),
    U16(u16),
    U32(u32),
    U64(u64),
    U128(u128),

    I8(i8),
    I16(i16),
    I32(i32),
    I64(i64),
    I128(i128),

    F32(f32),
    F64(f64),

    Char(char),
    String(String),
    Bytes(Vec<u8>),

    None,
    Some(Box<Content>),

    Unit,
    UnitStruct(&'static str),
    UnitVariant(&'static str, u32, &'static str),
    NewtypeStruct(&'static str, Box<Content>),
    NewtypeVariant(&'static str, u32, &'static str, Box<Content>),

    Seq(Vec<Content>),
    Tuple(Vec<Content>),
    TupleStruct(&'static str, Vec<Content>),
    TupleVariant(&'static str, u32, &'static str, Vec<Content>),
    Map(Vec<(Content, Content)>),
    Struct(&'static str, Vec<(&'static str, Content)>),
    StructVariant(
        &'static str,
        u32,
        &'static str,
        Vec<(&'static str, Content)>,
    ),
}

impl Serialize for Content {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match *self {
            Content::Bool(b) => serializer.serialize_bool(b),
            Content::U8(u) => serializer.serialize_u8(u),
            Content::U16(u) => serializer.serialize_u16(u),
            Content::U32(u) => serializer.serialize_u32(u),
            Content::U64(u) => serializer.serialize_u64(u),
            Content::U128(u) => serializer.serialize_u128(u),
            Content::I8(i) => serializer.serialize_i8(i),
            Content::I16(i) => serializer.serialize_i16(i),
            Content::I32(i) => serializer.serialize_i32(i),
            Content::I64(i) => serializer.serialize_i64(i),
            Content::I128(i) => serializer.serialize_i128(i),
            Content::F32(f) => serializer.serialize_f32(f),
            Content::F64(f) => serializer.serialize_f64(f),
            Content::Char(c) => serializer.serialize_char(c),
            Content::String(ref s) => serializer.serialize_str(s),
            Content::Bytes(ref b) => serializer.serialize_bytes(b),
            Content::None => serializer.serialize_none(),
            Content::Some(ref c) => serializer.serialize_some(&**c),
            Content::Unit => serializer.serialize_unit(),
            Content::UnitStruct(n) => serializer.serialize_unit_struct(n),
            Content::UnitVariant(n, i, v) => serializer.serialize_unit_variant(n, i, v),
            Content::NewtypeStruct(n, ref c) => serializer.serialize_newtype_struct(n, &**c),
            Content::NewtypeVariant(n, i, v, ref c) => {
                serializer.serialize_newtype_variant(n, i, v, &**c)
            }
            Content::Seq(ref elements) => elements.serialize(serializer),
            Content::Tuple(ref elements) => {
                use serde::ser::SerializeTuple;
                let mut tuple = serializer.serialize_tuple(elements.len())?;
                for e in elements {
                    tuple.serialize_element(e)?;
                }
                tuple.end()
            }
            Content::TupleStruct(n, ref fields) => {
                use serde::ser::SerializeTupleStruct;
                let mut ts = serializer.serialize_tuple_struct(n, fields.len())?;
                for f in fields {
                    ts.serialize_field(f)?;
                }
                ts.end()
            }
            Content::TupleVariant(n, i, v, ref fields) => {
                use serde::ser::SerializeTupleVariant;
                let mut tv = serializer.serialize_tuple_variant(n, i, v, fields.len())?;
                for f in fields {
                    tv.serialize_field(f)?;
                }
                tv.end()
            }
            Content::Map(ref entries) => {
                use serde::ser::SerializeMap;
                let mut map = serializer.serialize_map(Some(entries.len()))?;
                for &(ref k, ref v) in entries {
                    map.serialize_entry(k, v)?;
                }
                map.end()
            }
            Content::Struct(n, ref fields) => {
                use serde::ser::SerializeStruct;
                let mut s = serializer.serialize_struct(n, fields.len())?;
                for &(k, ref v) in fields {
                    s.serialize_field(k, v)?;
                }
                s.end()
            }
            Content::StructVariant(n, i, v, ref fields) => {
                use serde::ser::SerializeStructVariant;
                let mut sv = serializer.serialize_struct_variant(n, i, v, fields.len())?;
                for &(k, ref v) in fields {
                    sv.serialize_field(k, v)?;
                }
                sv.end()
            }
        }
    }
}

pub(crate) struct ContentSerializer<E> {
    is_human_readable: bool,
    error: PhantomData<E>,
}

impl<E> ContentSerializer<E> {
    pub(crate) fn new(is_human_readable: bool) -> Self {
        ContentSerializer {
            is_human_readable,
            error: PhantomData,
        }
    }
}

impl<E> Default for ContentSerializer<E> {
    fn default() -> Self {
        Self::new(true)
    }
}

impl<E> Serializer for ContentSerializer<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    type SerializeSeq = SerializeSeq<E>;
    type SerializeTuple = SerializeTuple<E>;
    type SerializeTupleStruct = SerializeTupleStruct<E>;
    type SerializeTupleVariant = SerializeTupleVariant<E>;
    type SerializeMap = SerializeMap<E>;
    type SerializeStruct = SerializeStruct<E>;
    type SerializeStructVariant = SerializeStructVariant<E>;

    fn is_human_readable(&self) -> bool {
        self.is_human_readable
    }

    fn serialize_bool(self, v: bool) -> Result<Content, E> {
        Ok(Content::Bool(v))
    }

    fn serialize_i8(self, v: i8) -> Result<Content, E> {
        Ok(Content::I8(v))
    }

    fn serialize_i16(self, v: i16) -> Result<Content, E> {
        Ok(Content::I16(v))
    }

    fn serialize_i32(self, v: i32) -> Result<Content, E> {
        Ok(Content::I32(v))
    }

    fn serialize_i64(self, v: i64) -> Result<Content, E> {
        Ok(Content::I64(v))
    }

    fn serialize_i128(self, v: i128) -> Result<Content, E> {
        Ok(Content::I128(v))
    }

    fn serialize_u8(self, v: u8) -> Result<Content, E> {
        Ok(Content::U8(v))
    }

    fn serialize_u16(self, v: u16) -> Result<Content, E> {
        Ok(Content::U16(v))
    }

    fn serialize_u32(self, v: u32) -> Result<Content, E> {
        Ok(Content::U32(v))
    }

    fn serialize_u64(self, v: u64) -> Result<Content, E> {
        Ok(Content::U64(v))
    }

    fn serialize_u128(self, v: u128) -> Result<Content, E> {
        Ok(Content::U128(v))
    }

    fn serialize_f32(self, v: f32) -> Result<Content, E> {
        Ok(Content::F32(v))
    }

    fn serialize_f64(self, v: f64) -> Result<Content, E> {
        Ok(Content::F64(v))
    }

    fn serialize_char(self, v: char) -> Result<Content, E> {
        Ok(Content::Char(v))
    }

    fn serialize_str(self, value: &str) -> Result<Content, E> {
        Ok(Content::String(value.to_owned()))
    }

    fn serialize_bytes(self, value: &[u8]) -> Result<Content, E> {
        Ok(Content::Bytes(value.to_owned()))
    }

    fn serialize_none(self) -> Result<Content, E> {
        Ok(Content::None)
    }

    fn serialize_some<T: ?Sized>(self, value: &T) -> Result<Content, E>
    where
        T: Serialize,
    {
        Ok(Content::Some(Box::new(value.serialize(self)?)))
    }

    fn serialize_unit(self) -> Result<Content, E> {
        Ok(Content::Unit)
    }

    fn serialize_unit_struct(self, name: &'static str) -> Result<Content, E> {
        Ok(Content::UnitStruct(name))
    }

    fn serialize_unit_variant(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
    ) -> Result<Content, E> {
        Ok(Content::UnitVariant(name, variant_index, variant))
    }

    fn serialize_newtype_struct<T: ?Sized>(
        self,
        name: &'static str,
        value: &T,
    ) -> Result<Content, E>
    where
        T: Serialize,
    {
        Ok(Content::NewtypeStruct(
            name,
            Box::new(value.serialize(self)?),
        ))
    }

    fn serialize_newtype_variant<T: ?Sized>(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
        value: &T,
    ) -> Result<Content, E>
    where
        T: Serialize,
    {
        Ok(Content::NewtypeVariant(
            name,
            variant_index,
            variant,
            Box::new(value.serialize(self)?),
        ))
    }

    fn serialize_seq(self, len: Option<usize>) -> Result<Self::SerializeSeq, E> {
        Ok(SerializeSeq {
            is_human_readable: self.is_human_readable,
            elements: Vec::with_capacity(len.unwrap_or(0)),
            error: PhantomData,
        })
    }

    fn serialize_tuple(self, len: usize) -> Result<Self::SerializeTuple, E> {
        Ok(SerializeTuple {
            is_human_readable: self.is_human_readable,
            elements: Vec::with_capacity(len),
            error: PhantomData,
        })
    }

    fn serialize_tuple_struct(
        self,
        name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleStruct, E> {
        Ok(SerializeTupleStruct {
            is_human_readable: self.is_human_readable,
            name,
            fields: Vec::with_capacity(len),
            error: PhantomData,
        })
    }

    fn serialize_tuple_variant(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleVariant, E> {
        Ok(SerializeTupleVariant {
            is_human_readable: self.is_human_readable,
            name,
            variant_index,
            variant,
            fields: Vec::with_capacity(len),
            error: PhantomData,
        })
    }

    fn serialize_map(self, len: Option<usize>) -> Result<Self::SerializeMap, E> {
        Ok(SerializeMap {
            is_human_readable: self.is_human_readable,
            entries: Vec::with_capacity(len.unwrap_or(0)),
            key: None,
            error: PhantomData,
        })
    }

    fn serialize_struct(self, name: &'static str, len: usize) -> Result<Self::SerializeStruct, E> {
        Ok(SerializeStruct {
            is_human_readable: self.is_human_readable,
            name,
            fields: Vec::with_capacity(len),
            error: PhantomData,
        })
    }

    fn serialize_struct_variant(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeStructVariant, E> {
        Ok(SerializeStructVariant {
            is_human_readable: self.is_human_readable,
            name,
            variant_index,
            variant,
            fields: Vec::with_capacity(len),
            error: PhantomData,
        })
    }
}

pub(crate) struct SerializeSeq<E> {
    is_human_readable: bool,
    elements: Vec<Content>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeSeq for SerializeSeq<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_element<T: ?Sized>(&mut self, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.elements.push(value);
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::Seq(self.elements))
    }
}

pub(crate) struct SerializeTuple<E> {
    is_human_readable: bool,
    elements: Vec<Content>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeTuple for SerializeTuple<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_element<T: ?Sized>(&mut self, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.elements.push(value);
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::Tuple(self.elements))
    }
}

pub(crate) struct SerializeTupleStruct<E> {
    is_human_readable: bool,
    name: &'static str,
    fields: Vec<Content>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeTupleStruct for SerializeTupleStruct<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_field<T: ?Sized>(&mut self, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.fields.push(value);
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::TupleStruct(self.name, self.fields))
    }
}

pub(crate) struct SerializeTupleVariant<E> {
    is_human_readable: bool,
    name: &'static str,
    variant_index: u32,
    variant: &'static str,
    fields: Vec<Content>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeTupleVariant for SerializeTupleVariant<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_field<T: ?Sized>(&mut self, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.fields.push(value);
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::TupleVariant(
            self.name,
            self.variant_index,
            self.variant,
            self.fields,
        ))
    }
}

pub(crate) struct SerializeMap<E> {
    is_human_readable: bool,
    entries: Vec<(Content, Content)>,
    key: Option<Content>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeMap for SerializeMap<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_key<T: ?Sized>(&mut self, key: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let key = key.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.key = Some(key);
        Ok(())
    }

    fn serialize_value<T: ?Sized>(&mut self, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let key = self
            .key
            .take()
            .expect("serialize_value called before serialize_key");
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.entries.push((key, value));
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::Map(self.entries))
    }

    fn serialize_entry<K: ?Sized, V: ?Sized>(&mut self, key: &K, value: &V) -> Result<(), E>
    where
        K: Serialize,
        V: Serialize,
    {
        let key = key.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.entries.push((key, value));
        Ok(())
    }
}

pub(crate) struct SerializeStruct<E> {
    is_human_readable: bool,
    name: &'static str,
    fields: Vec<(&'static str, Content)>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeStruct for SerializeStruct<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_field<T: ?Sized>(&mut self, key: &'static str, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.fields.push((key, value));
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::Struct(self.name, self.fields))
    }
}

pub(crate) struct SerializeStructVariant<E> {
    is_human_readable: bool,
    name: &'static str,
    variant_index: u32,
    variant: &'static str,
    fields: Vec<(&'static str, Content)>,
    error: PhantomData<E>,
}

impl<E> ser::SerializeStructVariant for SerializeStructVariant<E>
where
    E: ser::Error,
{
    type Ok = Content;
    type Error = E;

    fn serialize_field<T: ?Sized>(&mut self, key: &'static str, value: &T) -> Result<(), E>
    where
        T: Serialize,
    {
        let value = value.serialize(ContentSerializer::<E>::new(self.is_human_readable))?;
        self.fields.push((key, value));
        Ok(())
    }

    fn end(self) -> Result<Content, E> {
        Ok(Content::StructVariant(
            self.name,
            self.variant_index,
            self.variant,
            self.fields,
        ))
    }
}
