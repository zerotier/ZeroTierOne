pub mod crypto;
pub mod util;
pub mod error;
pub mod vl1;
pub mod vl2;
pub mod defaults;

pub const VERSION_MAJOR: u8 = 1;
pub const VERSION_MINOR: u8 = 99;
pub const VERSION_REVISION: u8 = 1;
pub const VERSION_STR: &'static str = "1.99.1";

/*
 * Protocol versions
 *
 * 1  - 0.2.0 ... 0.2.5
 * 2  - 0.3.0 ... 0.4.5
 *    + Added signature and originating peer to multicast frame
 *    + Double size of multicast frame bloom filter
 * 3  - 0.5.0 ... 0.6.0
 *    + Yet another multicast redesign
 *    + New crypto completely changes key agreement cipher
 * 4  - 0.6.0 ... 1.0.6
 *    + BREAKING CHANGE: New identity format based on hashcash design
 * 5  - 1.1.0 ... 1.1.5
 *    + Supports echo
 *    + Supports in-band world (root server definition) updates
 *    + Clustering! (Though this will work with protocol v4 clients.)
 *    + Otherwise backward compatible with protocol v4
 * 6  - 1.1.5 ... 1.1.10
 *    + Network configuration format revisions including binary values
 * 7  - 1.1.10 ... 1.1.17
 *    + Introduce trusted paths for local SDN use
 * 8  - 1.1.17 ... 1.2.0
 *    + Multipart network configurations for large network configs
 *    + Tags and Capabilities
 *    + inline push of CertificateOfMembership deprecated
 * 9  - 1.2.0 ... 1.2.14
 * 10 - 1.4.0 ... 1.4.6
 *    + Contained early pre-alpha versions of multipath, which are deprecated
 * 11 - 1.6.0 ... 2.0.0
 *    + Supports AES-GMAC-SIV symmetric crypto, backported from v2 tree.
 * 20 - 2.0.0 ... CURRENT
 *    + New more WAN-efficient P2P-assisted multicast algorithm
 *    + HELLO and OK(HELLO) include an extra HMAC to harden authentication
 *    + HELLO and OK(HELLO) carry meta-data in a dictionary that's encrypted
 *    + Forward secrecy, key lifetime management
 *    + Old planet/moon stuff is DEAD! Independent roots are easier.
 *    + AES encryption with the SIV construction AES-GMAC-SIV
 *    + New combined Curve25519/NIST P-384 identity type (type 1)
 *    + Short probe packets to reduce probe bandwidth
 *    + More aggressive NAT traversal techniques for IPv4 symmetric NATs
 */
pub const VERSION_PROTO: u8 = 20;
