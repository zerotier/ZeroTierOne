pub struct Address(pub u64);

impl Address {
    #[inline]
    pub fn new_from_string(s: &str) -> Address {
        return Address(u64::from_str_radix(s, 16).unwrap_or(0));
    }
}

impl ToString for Address {
    #[inline]
    fn to_string(&self) -> String {
        format!("{:0>10x}", self.0)
    }
}

impl serde::Serialize for Address {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct AddressVisitor;

impl<'de> serde::de::Visitor<'de> for AddressVisitor {
    type Value = Address;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Address in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(Address::new_from_string(s))
    }
}

impl<'de> serde::Deserialize<'de> for Address {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(AddressVisitor)
    }
}
