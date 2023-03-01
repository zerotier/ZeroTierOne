use serde_plain::{
    derive_deserialize_from_fromstr, derive_display_from_serialize,
    derive_fromstr_from_deserialize, derive_serialize_from_display,
};
use std::{fmt, num, str};

#[macro_use]
extern crate serde_derive;

#[derive(Serialize, Deserialize, Eq, PartialEq, Debug)]
#[serde(rename_all = "snake_case")]
pub enum Test {
    FooBarBaz,
    BlahBlah,
}

#[derive(Serialize, Deserialize, Eq, PartialEq, Debug)]
pub enum Test2 {
    FooBarBaz,
    BlahBlah,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Test2Error;

impl From<serde_plain::Error> for Test2Error {
    fn from(_: serde_plain::Error) -> Test2Error {
        Test2Error
    }
}

#[derive(Serialize, Deserialize, Eq, PartialEq, Debug)]
pub enum Test3 {
    FooBarBaz,
    BlahBlah,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Test3Error(String);

pub struct TestStruct(u32);

impl str::FromStr for TestStruct {
    type Err = num::ParseIntError;
    fn from_str(value: &str) -> Result<TestStruct, Self::Err> {
        Ok(TestStruct(value.parse()?))
    }
}

impl fmt::Display for TestStruct {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

derive_fromstr_from_deserialize!(Test);
derive_display_from_serialize!(Test);

derive_fromstr_from_deserialize!(Test2, Test2Error);
derive_display_from_serialize!(Test2);

derive_fromstr_from_deserialize!(Test3, |err| -> Test3Error { Test3Error(err.to_string()) });
derive_display_from_serialize!(Test3);

derive_deserialize_from_fromstr!(TestStruct, "valid positive number");
derive_serialize_from_display!(TestStruct);

#[test]
fn test_forward_basics() {
    assert_eq!(Test::FooBarBaz.to_string(), "foo_bar_baz");
    assert_eq!("foo_bar_baz".parse::<Test>().unwrap(), Test::FooBarBaz);
}

#[test]
fn test_forward_custom_error() {
    assert_eq!("whatever".parse::<Test2>(), Err(Test2Error));
}

#[test]
fn test_forward_custom_error_conversion() {
    assert_eq!(
        "whatever".parse::<Test3>(),
        Err(Test3Error(
            "unknown variant `whatever`, expected `FooBarBaz` or `BlahBlah`".to_string()
        ))
    );
}

#[test]
fn test_derive_deserialize() {
    let test: TestStruct = serde_plain::from_str("42").unwrap();
    assert_eq!(TestStruct(42).0, test.0);
}

#[test]
fn test_derive_serialize() {
    let test = serde_plain::to_string(&TestStruct(42)).unwrap();
    assert_eq!("42", test.as_str());
}
