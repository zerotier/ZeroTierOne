#![doc(html_root_url="https://docs.rs/serde-value/0.7.0/")]

use std::collections::BTreeMap;
use std::cmp::Ordering;
use std::hash::{Hash, Hasher};
use serde::Deserialize;
use ordered_float::OrderedFloat;

pub use de::*;
pub use ser::*;

mod de;
mod ser;

#[derive(Clone, Debug)]
pub enum Value {
    Bool(bool),

    U8(u8),
    U16(u16),
    U32(u32),
    U64(u64),

    I8(i8),
    I16(i16),
    I32(i32),
    I64(i64),

    F32(f32),
    F64(f64),

    Char(char),
    String(String),

    Unit,
    Option(Option<Box<Value>>),
    Newtype(Box<Value>),
    Seq(Vec<Value>),
    Map(BTreeMap<Value, Value>),
    Bytes(Vec<u8>),
}

impl Hash for Value {
    fn hash<H>(&self, hasher: &mut H)
    where
        H: Hasher
    {
        self.discriminant().hash(hasher);
        match *self {
            Value::Bool(v) => v.hash(hasher),
            Value::U8(v) => v.hash(hasher),
            Value::U16(v) => v.hash(hasher),
            Value::U32(v) => v.hash(hasher),
            Value::U64(v) => v.hash(hasher),
            Value::I8(v) => v.hash(hasher),
            Value::I16(v) => v.hash(hasher),
            Value::I32(v) => v.hash(hasher),
            Value::I64(v) => v.hash(hasher),
            Value::F32(v) => OrderedFloat(v).hash(hasher),
            Value::F64(v) => OrderedFloat(v).hash(hasher),
            Value::Char(v) => v.hash(hasher),
            Value::String(ref v) => v.hash(hasher),
            Value::Unit => ().hash(hasher),
            Value::Option(ref v) => v.hash(hasher),
            Value::Newtype(ref v) => v.hash(hasher),
            Value::Seq(ref v) => v.hash(hasher),
            Value::Map(ref v) => v.hash(hasher),
            Value::Bytes(ref v) => v.hash(hasher),
        }
    }
}

impl PartialEq for Value {
    fn eq(&self, rhs: &Self) -> bool {
        match (self, rhs) {
            (&Value::Bool(v0), &Value::Bool(v1)) if v0 == v1 => true,
            (&Value::U8(v0), &Value::U8(v1)) if v0 == v1 => true,
            (&Value::U16(v0), &Value::U16(v1)) if v0 == v1 => true,
            (&Value::U32(v0), &Value::U32(v1)) if v0 == v1 => true,
            (&Value::U64(v0), &Value::U64(v1)) if v0 == v1 => true,
            (&Value::I8(v0), &Value::I8(v1)) if v0 == v1 => true,
            (&Value::I16(v0), &Value::I16(v1)) if v0 == v1 => true,
            (&Value::I32(v0), &Value::I32(v1)) if v0 == v1 => true,
            (&Value::I64(v0), &Value::I64(v1)) if v0 == v1 => true,
            (&Value::F32(v0), &Value::F32(v1)) if OrderedFloat(v0) == OrderedFloat(v1) => true,
            (&Value::F64(v0), &Value::F64(v1)) if OrderedFloat(v0) == OrderedFloat(v1) => true,
            (&Value::Char(v0), &Value::Char(v1)) if v0 == v1 => true,
            (&Value::String(ref v0), &Value::String(ref v1)) if v0 == v1 => true,
            (&Value::Unit, &Value::Unit) => true,
            (&Value::Option(ref v0), &Value::Option(ref v1)) if v0 == v1 => true,
            (&Value::Newtype(ref v0), &Value::Newtype(ref v1)) if v0 == v1 => true,
            (&Value::Seq(ref v0), &Value::Seq(ref v1)) if v0 == v1 => true,
            (&Value::Map(ref v0), &Value::Map(ref v1)) if v0 == v1 => true,
            (&Value::Bytes(ref v0), &Value::Bytes(ref v1)) if v0 == v1 => true,
            _ => false,
        }
    }
}

impl Ord for Value {
    fn cmp(&self, rhs: &Self) -> Ordering {
        match (self, rhs) {
            (&Value::Bool(v0), &Value::Bool(ref v1)) => v0.cmp(v1),
            (&Value::U8(v0), &Value::U8(ref v1)) => v0.cmp(v1),
            (&Value::U16(v0), &Value::U16(ref v1)) => v0.cmp(v1),
            (&Value::U32(v0), &Value::U32(ref v1)) => v0.cmp(v1),
            (&Value::U64(v0), &Value::U64(ref v1)) => v0.cmp(v1),
            (&Value::I8(v0), &Value::I8(ref v1)) => v0.cmp(v1),
            (&Value::I16(v0), &Value::I16(ref v1)) => v0.cmp(v1),
            (&Value::I32(v0), &Value::I32(ref v1)) => v0.cmp(v1),
            (&Value::I64(v0), &Value::I64(ref v1)) => v0.cmp(v1),
            (&Value::F32(v0), &Value::F32(v1)) => OrderedFloat(v0).cmp(&OrderedFloat(v1)),
            (&Value::F64(v0), &Value::F64(v1)) => OrderedFloat(v0).cmp(&OrderedFloat(v1)),
            (&Value::Char(v0), &Value::Char(ref v1)) => v0.cmp(v1),
            (&Value::String(ref v0), &Value::String(ref v1)) => v0.cmp(v1),
            (&Value::Unit, &Value::Unit) => Ordering::Equal,
            (&Value::Option(ref v0), &Value::Option(ref v1)) => v0.cmp(v1),
            (&Value::Newtype(ref v0), &Value::Newtype(ref v1)) => v0.cmp(v1),
            (&Value::Seq(ref v0), &Value::Seq(ref v1)) => v0.cmp(v1),
            (&Value::Map(ref v0), &Value::Map(ref v1)) => v0.cmp(v1),
            (&Value::Bytes(ref v0), &Value::Bytes(ref v1)) => v0.cmp(v1),
            (ref v0, ref v1) => v0.discriminant().cmp(&v1.discriminant()),
        }
    }
}

impl Value {
    fn discriminant(&self) -> usize {
        match *self {
            Value::Bool(..) => 0,
            Value::U8(..) => 1,
            Value::U16(..) => 2,
            Value::U32(..) => 3,
            Value::U64(..) => 4,
            Value::I8(..) => 5,
            Value::I16(..) => 6,
            Value::I32(..) => 7,
            Value::I64(..) => 8,
            Value::F32(..) => 9,
            Value::F64(..) => 10,
            Value::Char(..) => 11,
            Value::String(..) => 12,
            Value::Unit => 13,
            Value::Option(..) => 14,
            Value::Newtype(..) => 15,
            Value::Seq(..) => 16,
            Value::Map(..) => 17,
            Value::Bytes(..) => 18,
        }
    }

    fn unexpected(&self) -> serde::de::Unexpected {
        match *self {
            Value::Bool(b) => serde::de::Unexpected::Bool(b),
            Value::U8(n) => serde::de::Unexpected::Unsigned(n as u64),
            Value::U16(n) => serde::de::Unexpected::Unsigned(n as u64),
            Value::U32(n) => serde::de::Unexpected::Unsigned(n as u64),
            Value::U64(n) => serde::de::Unexpected::Unsigned(n),
            Value::I8(n) => serde::de::Unexpected::Signed(n as i64),
            Value::I16(n) => serde::de::Unexpected::Signed(n as i64),
            Value::I32(n) => serde::de::Unexpected::Signed(n as i64),
            Value::I64(n) => serde::de::Unexpected::Signed(n),
            Value::F32(n) => serde::de::Unexpected::Float(n as f64),
            Value::F64(n) => serde::de::Unexpected::Float(n),
            Value::Char(c) => serde::de::Unexpected::Char(c),
            Value::String(ref s) => serde::de::Unexpected::Str(s),
            Value::Unit => serde::de::Unexpected::Unit,
            Value::Option(_) => serde::de::Unexpected::Option,
            Value::Newtype(_) => serde::de::Unexpected::NewtypeStruct,
            Value::Seq(_) => serde::de::Unexpected::Seq,
            Value::Map(_) => serde::de::Unexpected::Map,
            Value::Bytes(ref b) => serde::de::Unexpected::Bytes(b),
        }
    }

    pub fn deserialize_into<'de, T: Deserialize<'de>>(self) -> Result<T, DeserializerError> {
        T::deserialize(self)
    }
}

impl Eq for Value { }
impl PartialOrd for Value {
    fn partial_cmp(&self, rhs: &Self) -> Option<Ordering> {
        Some(self.cmp(rhs))
    }
}

#[cfg(test)]
use serde_derive::{Deserialize, Serialize};

#[test]
fn de_smoke_test() {
    // some convoluted Value
    let value = Value::Option(Some(Box::new(Value::Seq(vec![
        Value::U16(8),
        Value::Char('a'),
        Value::F32(1.0),
        Value::String("hello".into()),
        Value::Map(vec![
            (Value::Bool(false), Value::Unit),
            (Value::Bool(true), Value::Newtype(Box::new(
                Value::Bytes(b"hi".as_ref().into())
            ))),
        ].into_iter().collect()),
    ]))));

    // assert that the value remains unchanged through deserialization
    let value_de = Value::deserialize(value.clone()).unwrap();
    assert_eq!(value_de, value);
}

#[test]
fn ser_smoke_test() {
    #[derive(Serialize)]
    struct Foo {
        a: u32,
        b: String,
        c: Vec<bool>,
    }

    let foo = Foo {
        a: 15,
        b: "hello".into(),
        c: vec![true, false],
    };

    let expected = Value::Map(vec![
        (Value::String("a".into()), Value::U32(15)),
        (Value::String("b".into()), Value::String("hello".into())),
        (Value::String("c".into()), Value::Seq(vec![Value::Bool(true), Value::Bool(false)])),
    ].into_iter().collect());

    let value = to_value(&foo).unwrap();
    assert_eq!(expected, value);
}

#[test]
fn deserialize_into_enum() {
    #[derive(Deserialize, Debug, PartialEq, Eq)]
    enum Foo {
        Bar,
        Baz(u8),
    }

    let value = Value::String("Bar".into());
    assert_eq!(Foo::deserialize(value).unwrap(), Foo::Bar);

    let value = Value::Map(vec![
        (Value::String("Baz".into()), Value::U8(1))
    ].into_iter().collect());
    assert_eq!(Foo::deserialize(value).unwrap(), Foo::Baz(1));
}

#[test]
fn serialize_from_enum() {
    #[derive(Serialize)]
    enum Foo {
        Bar,
        Baz(u8),
        Qux { quux: u8 },
        Corge(u8, u8),
    }

    let bar = Foo::Bar;
    assert_eq!(to_value(&bar).unwrap(), Value::String("Bar".into()));

    let baz = Foo::Baz(1);
    assert_eq!(
        to_value(&baz).unwrap(),
        Value::Map(
            vec![(Value::String("Baz".into()), Value::U8(1))]
                .into_iter()
                .collect(),
        )
    );

    let qux = Foo::Qux { quux: 2 };
    assert_eq!(
        to_value(&qux).unwrap(),
        Value::Map(
            vec![(
                Value::String("Qux".into()),
                Value::Map(
                    vec![(Value::String("quux".into()), Value::U8(2))]
                        .into_iter()
                        .collect()
                )
            )]
            .into_iter()
            .collect()
        )
    );

    let corge = Foo::Corge(3, 4);
    assert_eq!(
        to_value(&corge).unwrap(),
        Value::Map(
            vec![(
                Value::String("Corge".into()),
                Value::Seq(vec![Value::U8(3), Value::U8(4)])
            )]
            .into_iter()
            .collect()
        )
    );
}

#[test]
fn deserialize_inside_deserialize_impl() {
    #[derive(Debug, PartialEq, Eq)]
    enum Event {
        Added(u32),
        Error(u8),
    }

    impl<'de> serde::Deserialize<'de> for Event {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
            #[derive(Deserialize)]
            struct RawEvent {
                kind: String,
                object: Value,
            }

            let raw_event = RawEvent::deserialize(deserializer)?;

            // Cannot directly use Value as Deserializer, since error type needs to be
            // generic D::Error rather than specific serde_value::DeserializerError
            let object_deserializer = ValueDeserializer::new(raw_event.object);

            Ok(match &*raw_event.kind {
                "ADDED" => Event::Added(<_>::deserialize(object_deserializer)?),
                "ERROR" => Event::Error(<_>::deserialize(object_deserializer)?),
                kind => return Err(serde::de::Error::unknown_variant(kind, &["ADDED", "ERROR"])),
            })
        }
    }

    let input = Value::Map(vec![
        (Value::String("kind".to_owned()), Value::String("ADDED".to_owned())),
        (Value::String("object".to_owned()), Value::U32(5)),
    ].into_iter().collect());
    let event = Event::deserialize(input).expect("could not deserialize ADDED event");
    assert_eq!(event, Event::Added(5));

    let input = Value::Map(vec![
        (Value::String("kind".to_owned()), Value::String("ERROR".to_owned())),
        (Value::String("object".to_owned()), Value::U8(5)),
    ].into_iter().collect());
    let event = Event::deserialize(input).expect("could not deserialize ERROR event");
    assert_eq!(event, Event::Error(5));

    let input = Value::Map(vec![
        (Value::String("kind".to_owned()), Value::String("ADDED".to_owned())),
        (Value::String("object".to_owned()), Value::Unit),
    ].into_iter().collect());
    let _ = Event::deserialize(input).expect_err("expected deserializing bad ADDED event to fail");
}

#[test]
fn deserialize_newtype() {
    #[derive(Debug, Deserialize, PartialEq)]
    struct Foo(i32);

    let input = Value::I32(5);
    let foo = Foo::deserialize(input).unwrap();
    assert_eq!(foo, Foo(5));
}

#[test]
fn deserialize_newtype2() {
    #[derive(Debug, Deserialize, PartialEq)]
    struct Foo(i32);

    #[derive(Debug, Deserialize, PartialEq)]
    struct Bar {
        foo: Foo,
    }

    let input = Value::Map(vec![
        (Value::String("foo".to_owned()), Value::I32(5))
    ].into_iter().collect());
    let bar = Bar::deserialize(input).unwrap();
    assert_eq!(bar, Bar { foo: Foo(5) });
}
