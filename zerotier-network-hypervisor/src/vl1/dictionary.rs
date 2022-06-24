// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::collections::BTreeMap;
use std::io::Write;

use crate::util::hex::HEX_CHARS;

const BOOL_TRUTH: &str = "1tTyY";

/// Dictionary is an extremely simple key=value serialization format.
///
/// It's designed for extreme parsing simplicity and is human readable if keys and values are strings.
/// It also supports binary keys and values which will be minimally escaped but render the result not
/// entirely human readable. Keys are serialized in natural sort order so the result can be consistently
/// checksummed or hashed.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dictionary(pub(crate) BTreeMap<String, Vec<u8>>);

fn write_escaped<W: Write>(b: &[u8], w: &mut W) -> std::io::Result<()> {
    let mut i = 0_usize;
    let l = b.len();
    while i < l {
        let ii = i + 1;
        match b[i] {
            0 => {
                w.write_all(&[b'\\', b'0'])?;
            }
            b'\n' => {
                w.write_all(&[b'\\', b'n'])?;
            }
            b'\r' => {
                w.write_all(&[b'\\', b'r'])?;
            }
            b'=' => {
                w.write_all(&[b'\\', b'e'])?;
            }
            b'\\' => {
                w.write_all(&[b'\\', b'\\'])?;
            }
            _ => {
                w.write_all(&b[i..ii])?;
            }
        }
        i = ii;
    }
    Ok(())
}

fn append_printable(s: &mut String, b: &[u8]) {
    for c in b {
        let c = *c as char;
        if c.is_alphanumeric() || c.is_whitespace() {
            s.push(c);
        } else {
            s.push('\\');
            s.push('x');
            s.push(HEX_CHARS[((c as u8) >> 4) as usize] as char);
            s.push(HEX_CHARS[((c as u8) & 0xf) as usize] as char);
        }
    }
}

impl Dictionary {
    pub fn new() -> Self {
        Self(BTreeMap::new())
    }

    pub fn clear(&mut self) {
        self.0.clear()
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.0.len()
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    pub fn get_str(&self, k: &str) -> Option<&str> {
        self.0.get(k).map_or(None, |v| std::str::from_utf8(v.as_slice()).map_or(None, |s| Some(s)))
    }

    pub fn get_bytes(&self, k: &str) -> Option<&[u8]> {
        self.0.get(k).map_or(None, |v| Some(v.as_slice()))
    }

    pub fn get_u64(&self, k: &str) -> Option<u64> {
        self.get_str(k).map_or(None, |s| u64::from_str_radix(s, 16).map_or(None, |i| Some(i)))
    }

    pub fn get_i64(&self, k: &str) -> Option<i64> {
        self.get_str(k).map_or(None, |s| i64::from_str_radix(s, 16).map_or(None, |i| Some(i)))
    }

    pub fn get_bool(&self, k: &str) -> Option<bool> {
        self.0.get(k).map_or(None, |v| v.first().map_or(Some(false), |c| Some(BOOL_TRUTH.contains(*c as char))))
    }

    pub fn set_str(&mut self, k: &str, v: &str) {
        let _ = self.0.insert(String::from(k), v.as_bytes().to_vec());
    }

    pub fn set_u64(&mut self, k: &str, v: u64) {
        let _ = self.0.insert(String::from(k), crate::util::hex::to_vec_u64(v, true));
    }

    pub fn set_bytes(&mut self, k: &str, v: Vec<u8>) {
        let _ = self.0.insert(String::from(k), v);
    }

    pub fn set_bool(&mut self, k: &str, v: bool) {
        let _ = self.0.insert(String::from(k), vec![if v { b'1' } else { b'0' }]);
    }

    /// Write a dictionary in transport format to a writer.
    pub fn write_to<W: Write>(&self, w: &mut W) -> std::io::Result<()> {
        for kv in self.0.iter() {
            write_escaped(kv.0.as_bytes(), w)?;
            w.write_all(&[b'='])?;
            write_escaped(kv.1.as_slice(), w)?;
            w.write_all(&[b'\n'])?;
        }
        Ok(())
    }

    /// Write a dictionary in transport format to a byte vector.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut b: Vec<u8> = Vec::new();
        b.reserve(32 * self.0.len());
        let _ = self.write_to(&mut b);
        b
    }

    /// Decode a dictionary in byte format, or return None if the input is invalid.
    pub fn from_bytes(b: &[u8]) -> Option<Dictionary> {
        let mut d = Dictionary::new();
        let mut kv: [Vec<u8>; 2] = [Vec::new(), Vec::new()];
        let mut state = 0;
        let mut escape = false;
        for c in b {
            let c = *c;
            if escape {
                escape = false;
                kv[state].push(match c {
                    b'0' => 0,
                    b'n' => b'\n',
                    b'r' => b'\r',
                    b'e' => b'=',
                    _ => c, // =, \, and escapes before other characters are unnecessary but not errors
                });
            } else if c == b'\\' {
                escape = true;
            } else if c == b'=' {
                if state != 0 {
                    return None;
                }
                state = 1;
            } else if c == b'\n' {
                if state != 1 {
                    return None;
                }
                state = 0;
                if !kv[0].is_empty() {
                    if String::from_utf8(kv[0].clone()).map_or(true, |key| {
                        d.0.insert(key, kv[1].clone());
                        false
                    }) {
                        return None;
                    }
                }
                kv[0].clear();
                kv[1].clear();
            } else if c != b'\r' {
                kv[state].push(c);
            }
        }
        Some(d)
    }

    pub fn iter(&self) -> impl Iterator<Item = (&String, &Vec<u8>)> {
        self.0.iter()
    }
}

impl ToString for Dictionary {
    /// Get the dictionary in an always readable format with non-printable characters replaced by '\xXX'.
    /// This is not a serializable output that can be re-imported. Use write_to() for that.
    fn to_string(&self) -> String {
        let mut s = String::new();
        for kv in self.0.iter() {
            append_printable(&mut s, kv.0.as_bytes());
            s.push('=');
            append_printable(&mut s, kv.1.as_slice());
            s.push('\n');
        }
        s
    }
}

#[cfg(test)]
mod tests {
    #[derive(PartialEq, Eq, Clone, Debug)]
    enum Type {
        String,
        Bytes,
        U64,
        Bool,
    }

    type TypeMap = HashMap<String, Type>;

    use std::collections::HashMap;

    use crate::vl1::dictionary::{Dictionary, BOOL_TRUTH};

    // from zeronsd
    pub fn randstring(len: u8) -> String {
        (0..len).map(|_| (rand::random::<u8>() % 26) + 'a' as u8).map(|c| if rand::random::<bool>() { (c as char).to_ascii_uppercase() } else { c as char }).map(|c| c.to_string()).collect::<Vec<String>>().join("")
    }

    fn make_dictionary() -> (Dictionary, TypeMap) {
        let mut d = Dictionary::new();
        let mut tm = TypeMap::new();

        for _ in 0..(rand::random::<usize>() % 20) + 1 {
            // NOTE: just doing this twice because I want to keep the code a little cleaner.
            let selection = rand::random::<usize>() % 4;

            let key = randstring(10);

            // set the key
            match selection {
                0 => d.set_str(&key, &randstring(10)),
                1 => d.set_u64(&key, rand::random()),
                2 => d.set_bytes(&key, (0..((rand::random::<usize>() % 10) + 1)).into_iter().map(|_| rand::random()).collect::<Vec<u8>>()),
                3 => d.set_bool(&key, rand::random::<bool>()),
                _ => unreachable!(),
            }

            match selection {
                0 => tm.insert(key, Type::String),
                1 => tm.insert(key, Type::U64),
                2 => tm.insert(key, Type::Bytes),
                3 => tm.insert(key, Type::Bool),
                _ => unreachable!(),
            };
        }

        (d, tm)
    }

    #[test]
    fn dictionary_basic() {
        let mut d = Dictionary::new();
        d.set_str("foo", "bar");
        d.set_u64("bar", 0xfeedcafebabebeef);
        d.set_bytes("baz", vec![1, 2, 3, 4, 5, 6, 7, 8, 9]);
        d.set_bool("lala", true);
        d.set_bool("haha", false);
        let bytes = d.to_bytes();
        let d2 = Dictionary::from_bytes(bytes.as_slice()).unwrap();
        assert!(d.eq(&d2));
    }

    #[test]
    fn dictionary_to_string() {
        for _ in 0..1000 {
            let (d, _) = make_dictionary();
            assert_ne!(d.to_string().len(), 0)
        }
    }

    #[test]
    fn dictionary_clear() {
        for _ in 0..1000 {
            let (mut d, _) = make_dictionary();
            assert_ne!(d.len(), 0);
            assert!(!d.is_empty());
            d.clear();
            assert!(d.is_empty());
            assert_eq!(d.len(), 0);
        }
    }

    #[test]
    fn dictionary_io() {
        for _ in 0..1000 {
            let (d, _) = make_dictionary();
            assert_ne!(d.len(), 0);
            assert!(!d.is_empty());

            let mut v = Vec::new();
            let mut cursor = std::io::Cursor::new(&mut v);
            assert!(d.write_to(&mut cursor).is_ok());
            drop(cursor);
            assert!(!v.is_empty());

            let d2 = super::Dictionary::from_bytes(v.as_slice());
            assert!(d2.is_some());
            let d2 = d2.unwrap();
            assert_eq!(d, d2);

            let d2 = super::Dictionary::from_bytes(&d.to_bytes());
            assert!(d2.is_some());
            let d2 = d2.unwrap();
            assert_eq!(d, d2);
        }
    }

    #[test]
    fn dictionary_accessors() {
        for _ in 0..1000 {
            let (d, tm) = make_dictionary();

            for (k, v) in d.iter() {
                match tm.get(k).unwrap() {
                    Type::String => {
                        let v2 = d.get_str(k);
                        assert!(v2.is_some());
                        assert_eq!(String::from_utf8(v.to_vec()).unwrap(), String::from(v2.unwrap()));
                    }
                    Type::Bytes => {
                        let v2 = d.get_bytes(k);
                        assert!(v2.is_some());
                        assert_eq!(v, v2.unwrap());
                    }
                    Type::Bool => {
                        let v2 = d.get_bool(k);
                        assert!(v2.is_some());
                        // FIXME move this lettering to a constant
                        assert_eq!(BOOL_TRUTH.contains(*v.iter().nth(0).unwrap() as char), v2.unwrap());
                    }
                    Type::U64 => {
                        let v2 = d.get_u64(k);
                        assert!(v2.is_some());

                        assert_eq!(u64::from_str_radix(d.get_str(k).unwrap(), 16).unwrap(), v2.unwrap());
                    }
                }
            }
        }
    }
}
