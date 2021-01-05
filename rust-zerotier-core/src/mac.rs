pub struct MAC(pub u64);

impl ToString for MAC {
    fn to_string(&self) -> String {
        let x = self.0;
        format!("{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}:{:0>2x}",
            (x >> 40) & 0xff,
            (x >> 32) & 0xff,
            (x >> 24) & 0xff,
            (x >> 16) & 0xff,
            (x >> 8) & 0xff,
            x & 0xff)
    }
}

impl From<&str> for MAC {
    fn from(s: &str) -> MAC {
        MAC(u64::from_str_radix(s.replace(":","").as_str(), 16).unwrap_or(0))
    }
}

impl serde::Serialize for MAC {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct AddressVisitor;

impl<'de> serde::de::Visitor<'de> for AddressVisitor {
    type Value = MAC;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("Ethernet MAC address in string format (with or without : separators)")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        Ok(MAC::from(s))
    }
}

impl<'de> serde::Deserialize<'de> for MAC {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(AddressVisitor)
    }
}
