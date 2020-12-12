pub struct Address(pub u64);

impl Address {
    pub fn new_from_string(s: &str) -> Self {
        Address(u64::from_str_radix(s, 16).unwrap_or(0))
    }

    pub fn to_u64(&self) -> u64 {
        self.0
    }
}

impl ToString for Address {
    fn to_string(&self) -> String {
        format!("{:0>10x}", self.0)
    }
}

impl From<String> for Address {
    fn from(s: String) -> Self {
        Address::new_from_string(s.as_str())
    }
}
