//! DNS resolution

pub use resolve::{Addrs, Resolve, Resolving};
pub(crate) use resolve::{DnsResolverWithOverrides, DynResolver};

pub(crate) mod gai;
pub(crate) mod resolve;
#[cfg(feature = "trust-dns")]
pub(crate) mod trust_dns;
