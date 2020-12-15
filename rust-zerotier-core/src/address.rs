pub type Address = u64;

pub fn address_to_string(a: Address) -> String {
    format!("{:0>10x}", a as u64)
}

pub fn address_from_string(s: &str) -> Address {
    return u64::from_str_radix(s, 16).unwrap_or(0) as Address;
}
