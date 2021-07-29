use std::collections::BTreeMap;
use std::io::Write;

use crate::util::hex::HEX_CHARS;

/// Dictionary is an extremely simple key=value serialization format.
/// It's designed for extreme parsing simplicity and is human readable if keys and values are strings.
/// It also supports binary keys and values which will be minimally escaped but render the result not
/// entirely human readable. Keys are serialized in natural sort order so the result can be consistently
/// checksummed or hashed.
#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Dictionary(BTreeMap<String, Vec<u8>>);

impl Default for Dictionary {
    #[inline(always)]
    fn default() -> Self {
        Self(BTreeMap::new())
    }
}

fn write_escaped<W: Write>(b: &[u8], w: &mut W) -> std::io::Result<()> {
    let mut i = 0_usize;
    let l = b.len();
    while i < l {
        let ii = i + 1;
        match b[i] {
            0 => { w.write_all(&[b'\\', b'0'])?; }
            b'\n' => { w.write_all(&[b'\\', b'n'])?; }
            b'\r' => { w.write_all(&[b'\\', b'r'])?; }
            b'=' => { w.write_all(&[b'\\', b'e'])?; }
            b'\\' => { w.write_all(&[b'\\', b'\\'])?; }
            _ => { w.write_all(&b[i..ii])?; }
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
    #[inline(always)]
    pub fn new() -> Self {
        Self(BTreeMap::new())
    }

    #[inline(always)]
    pub fn clear(&mut self) {
        self.0.clear()
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.0.len()
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
        self.0.get(k).map_or(None, |v| {
            if v.is_empty() {
                Some(false)
            } else {
                Some(match v[0] {
                    b'1' | b't' | b'T' | b'y' | b'Y' => true,
                    _ => false
                })
            }
        })
    }

    #[inline(always)]
    pub fn set_str(&mut self, k: &str, v: &str) {
        let _ = self.0.insert(String::from(k), v.as_bytes().to_vec());
    }

    #[inline(always)]
    pub fn set_u64(&mut self, k: &str, v: u64) {
        let _ = self.0.insert(String::from(k), crate::util::hex::to_vec_u64(v, true));
    }

    #[inline(always)]
    pub fn set_bytes(&mut self, k: &str, v: Vec<u8>) {
        let _ = self.0.insert(String::from(k), v);
    }

    #[inline(always)]
    pub fn set_bool(&mut self, k: &str, v: bool) {
        let _ = self.0.insert(String::from(k), (if v { [b'1'] } else { [b'0'] }).to_vec());
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
                    _ => c // =, \, and escapes before other characters are unnecessary but not errors
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
}

impl ToString for Dictionary {
    /// Get the dictionary in an always readable format with non-printable characters replaced by '\xXX'.
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
    use crate::vl1::dictionary::Dictionary;

    #[test]
    fn dictionary() {
        let mut d = Dictionary::new();
        d.set_str("foo", "bar");
        d.set_u64("bar", 0xfeedcafebabebeef);
        d.set_bytes("baz", vec![1,2,3,4,5,6,7,8,9]);
        d.set_bool("lala", true);
        d.set_bool("haha", false);
        let bytes = d.to_bytes();
        let d2 = Dictionary::from_bytes(bytes.as_slice()).unwrap();
        assert!(d.eq(&d2));
    }
}
