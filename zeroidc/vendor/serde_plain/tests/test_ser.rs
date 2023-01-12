#[macro_use]
extern crate serde_derive;

use std::fmt;

#[derive(Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Test {
    FooBarBaz,
    BlahBlah,
}

#[derive(Serialize)]
pub struct NewInt(i32);

impl fmt::Display for Test {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", serde_plain::to_string(self).unwrap())
    }
}

impl ToString for NewInt {
    fn to_string(&self) -> String {
        serde_plain::to_string(self).unwrap()
    }
}

#[test]
fn test_basics() {
    assert_eq!(serde_plain::to_string(&42).unwrap(), "42");
    assert_eq!(serde_plain::to_string(&"blafasel").unwrap(), "blafasel");
    assert_eq!(
        serde_plain::to_string(&Test::FooBarBaz).unwrap(),
        "foo_bar_baz"
    );
    assert_eq!(
        serde_plain::to_string(&Test::BlahBlah).unwrap(),
        "blah_blah"
    );
    assert_eq!(serde_plain::to_string(&NewInt(42)).unwrap(), "42");
    assert_eq!(serde_plain::to_string(&Some(true)).unwrap(), "true");
    assert_eq!(serde_plain::to_string(&None::<()>).unwrap(), "");
    assert_eq!(serde_plain::to_string(&()).unwrap(), "");
}

#[test]
fn test_to_string() {
    assert_eq!(Test::FooBarBaz.to_string(), "foo_bar_baz");
    assert_eq!(NewInt(42).to_string(), "42");
}
