pub struct NetworkId(pub u64);

impl NetworkId {
    #[inline(always)]
    pub fn new_from_string(s: &str) -> NetworkId {
        return NetworkId(u64::from_str_radix(s, 16).unwrap_or(0));
    }
}

impl ToString for NetworkId {
    #[inline(always)]
    fn to_string(&self) -> String {
        format!("{:0>16x}", self.0)
    }
}

impl From<u64> for NetworkId {
    #[inline(always)]
    fn from(n: u64) -> Self {
        NetworkId(n)
    }
}

impl From<&str> for NetworkId {
    #[inline(always)]
    fn from(s: &str) -> Self {
        NetworkId::new_from_string(s)
    }
}

impl serde::Serialize for NetworkId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct NetworkIdVisitor;

impl<'de> serde::de::Visitor<'de> for NetworkIdVisitor {
    type Value = NetworkId;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Address in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(NetworkId::new_from_string(s))
    }
}

impl<'de> serde::Deserialize<'de> for NetworkId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(NetworkIdVisitor)
    }
}
