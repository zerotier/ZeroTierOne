pub type NetworkId = u64;

pub fn network_id_to_string(n: NetworkId) -> String {
    format!("{:0>16x}", n as u64)
}

pub fn network_id_from_string(s: &str) -> NetworkId {
    return u64::from_str_radix(s, 16).unwrap_or(0) as NetworkId;
}
