use core::mem::ManuallyDrop;

#[allow(dead_code)]
struct NetworkRule {
    /**
     * Type and flags
     *
     * Bits are: NOTTTTTT
     *
     * N - If true, sense of match is inverted (no effect on actions)
     * O - If true, result is ORed with previous instead of ANDed (no effect on actions)
     * T - Rule or action type
     *
     * AND with 0x3f to get type, 0x80 to get NOT bit, and 0x40 to get OR bit.
     */
    t: u8,
    v: NetworkRuleValue,
}

#[allow(dead_code)]
union NetworkRuleValue {
    ipv4: ManuallyDrop<IPv4>,
    ipv6: ManuallyDrop<IPv6>,
    int_range: ManuallyDrop<IntRange>,

    /**
     * Packet characteristic flags being matched
     */
    characteristics: u64,

    /**
     * IP port range -- start-end inclusive -- host byte order
     */
    port_range: [u16; 2],

    /**
     * 40-bit ZeroTier address (in least significant bits, host byte order)
     */
    zt: u64,

    /**
     * 0 = never, UINT32_MAX = always
     */
    random_probability: u32,

    /**
     * 48-bit Ethernet MAC address in big-endian order
     */
    mac: [u8; 6],

    /**
     * VLAN ID in host byte order
     */
    vlan_id: u16,

    /**
     * VLAN PCP (least significant 3 bits)
     */
    vlan_pcp: u8,

    /**
     * VLAN DEI (single bit / boolean)
     */
    vlan_dei: u8,

    /**
     * Ethernet type in host byte order
     */
    ethertype: u16,

    /**
     * IP protocol
     */
    ip_protocol: u8,

    ip_tos: ManuallyDrop<IPTOS>,

    frame_size: [u16; 2],

    icmp: ManuallyDrop<ICMP>,
    tag: ManuallyDrop<Tag>,

    /**
     * Quality of Service (QoS) bucket we want a frame to be placed in
     */
    qos_bucket: u8,
}

/**
 * IPv6 address in big-endian / network byte order and netmask bits
 */
#[allow(dead_code)]
struct IPv6 {
    ip: [u8; 16],
    mask: u8,
}

/**
 * IPv4 address in big-endian / network byte order
 */
#[allow(dead_code)]
struct IPv4 {
    ip: u32,
    mask: u8,
}

/**
 * Integer range match in packet payload
 *
 * This allows matching of ranges of integers up to 64 bits wide where
 * the range is +/- INT32_MAX. It's packed this way so it fits in 16
 * bytes and doesn't enlarge the overall size of this union.
 */
#[allow(dead_code)]
struct IntRange {
    start: u64, // integer range start
    end: u32,   // end of integer range (relative to start, inclusive, 0 for equality w/start)
    idx: u16,   // index in packet of integer
    format: u8, // bits in integer (range 1-64, ((format&63)+1)) and endianness (MSB 1 for little, 0 for big)
}

/**
 * IP type of service a.k.a. DSCP field
 */
#[allow(dead_code)]
struct IPTOS {
    mask: u8,
    value: [u8; 2],
}

#[allow(dead_code)]
struct ICMP {
    typ: u8,
    code: u8,
    flags: u8,
}

#[allow(dead_code)]
struct Tag {
    id: u32,
    value: u32,
}

/**
 * Destinations for TEE and REDIRECT
 */
#[allow(dead_code)]
struct Fwd {
    address: u64,
    flags: u32,
    length: u16,
}
