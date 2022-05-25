// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::borrow::Cow;

/// A flat key/value store implemented in terms of arrays tuples of (key, value).
#[repr(transparent)]
#[derive(Clone, PartialEq, Eq)]
pub struct FlatKV(Vec<(&'static str, Value)>);

/// Value variant for FlatKV.
#[derive(Clone, PartialEq, Eq)]
pub enum Value {
    N,
    KV(FlatKV),
    S(Cow<'static, str>),
    I(i64),
    UI(u64),
    B(bool),
    Endpoint(crate::vl1::Endpoint),
    Identity(crate::vl1::Identity),
}

impl Into<Value> for FlatKV {
    #[inline(always)]
    fn into(self) -> Value {
        Value::KV(self)
    }
}

impl Into<Value> for &'static str {
    #[inline(always)]
    fn into(self) -> Value {
        Value::S(Cow::Borrowed(self))
    }
}

impl Into<Value> for String {
    #[inline(always)]
    fn into(self) -> Value {
        Value::S(Cow::Owned(self))
    }
}

impl Into<Value> for i64 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::I(self)
    }
}

impl Into<Value> for u64 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::UI(self)
    }
}

impl Into<Value> for isize {
    #[inline(always)]
    fn into(self) -> Value {
        Value::I(self as i64)
    }
}

impl Into<Value> for usize {
    #[inline(always)]
    fn into(self) -> Value {
        Value::UI(self as u64)
    }
}

impl Into<Value> for i32 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::I(self as i64)
    }
}

impl Into<Value> for u32 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::UI(self as u64)
    }
}

impl Into<Value> for i16 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::I(self as i64)
    }
}

impl Into<Value> for u16 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::UI(self as u64)
    }
}

impl Into<Value> for i8 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::I(self as i64)
    }
}

impl Into<Value> for u8 {
    #[inline(always)]
    fn into(self) -> Value {
        Value::UI(self as u64)
    }
}

impl Into<Value> for bool {
    #[inline(always)]
    fn into(self) -> Value {
        Value::B(self)
    }
}

impl Into<Value> for crate::vl1::Endpoint {
    #[inline(always)]
    fn into(self) -> Value {
        Value::Endpoint(self)
    }
}

impl Into<Value> for crate::vl1::InetAddress {
    #[inline(always)]
    fn into(self) -> Value {
        Value::Endpoint(crate::vl1::Endpoint::IpUdp(self))
    }
}

impl Into<Value> for crate::vl1::Identity {
    #[inline(always)]
    fn into(self) -> Value {
        Value::Identity(self)
    }
}

impl ToString for Value {
    fn to_string(&self) -> String {
        match self {
            Value::N => "(null)".into(),
            Value::KV(x) => x.to_string(),
            Value::S(x) => x.to_string(),
            Value::I(x) => x.to_string(),
            Value::UI(x) => x.to_string(),
            Value::B(x) => x.to_string(),
            Value::Endpoint(x) => x.to_string(),
            Value::Identity(x) => x.to_string(),
        }
    }
}

impl FlatKV {
    #[inline(always)]
    pub fn add<T: Into<Value>>(&mut self, k: &'static str, v: T) {
        self.0.push((k, v.into()))
    }
}

fn json_escape(src: &str, escaped: &mut String) {
    use std::fmt::Write;
    let mut utf16_buf = [0u16; 2];
    for c in src.chars() {
        match c {
            '\x08' => escaped.push_str("\\b"),
            '\x0c' => escaped.push_str("\\f"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            '"' => escaped.push_str("\\\""),
            '\\' => escaped.push_str("\\\\"),
            '/' => escaped.push_str("\\/"),
            c if c.is_ascii_graphic() => escaped.push(c),
            c => {
                let encoded = c.encode_utf16(&mut utf16_buf);
                for utf16 in encoded {
                    write!(escaped, "\\u{:04X}", utf16).unwrap();
                }
            }
        }
    }
}

impl Default for FlatKV {
    #[inline(always)]
    fn default() -> Self {
        Self(Vec::new())
    }
}

impl FlatKV {
    #[inline(always)]
    pub fn new() -> Self {
        Self(Vec::new())
    }
}

impl ToString for FlatKV {
    /// Output a JSON formatted map of values or maps.
    fn to_string(&self) -> String {
        let mut first = true;
        let mut tmp = String::new();
        tmp.push_str("{ "); //} //"
        for (k, v) in self.0.iter() {
            if first {
                first = false;
            } else {
                tmp.push_str(", ");
            }
            tmp.push('"');
            json_escape(*k, &mut tmp);
            tmp.push_str("\": ");
            match v {
                Value::S(_) | Value::Endpoint(_) | Value::Identity(_) => {
                    tmp.push('"');
                    json_escape(v.to_string().as_str(), &mut tmp);
                    tmp.push('"');
                }
                _ => tmp.push_str(v.to_string().as_str()),
            }
        }
        tmp.push_str("} ");
        tmp
    }
}

#[macro_export]
macro_rules! kv {
    ($($key:expr => $value:expr,)+) => (kv!($($key => $value),+));
    ( $($key:expr => $value:expr),* ) => {
        {
            #[allow(unused_mut)]
            let mut _kv = crate::util::flatkv::FlatKV(Vec::new());
            $(
                _kv.add($key, $value);
            )*
            _kv
        }
    };
}

#[cfg(test)]
mod tests {
    #[test]
    fn kv_macro() {
        let kv = kv!(
            "foo" => 0_u64,
            "bar" => "bar",
            "baz" => -1_i64,
            "lala" => false,
            "lol" => kv!(
                "boo" => 1_u16,
                "far" => 2_u32,
            )
        );
    }
}
