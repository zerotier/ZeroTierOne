use std::borrow::Cow;

use zerotier_utils::blob::Blob;
use zerotier_utils::flatsortedmap::FlatSortedMap;

use serde::{Deserialize, Serialize};

use crate::vl1::identity::IDENTITY_FINGERPRINT_SIZE;
use crate::vl1::inetaddress::InetAddress;
use crate::vl2::rule::Rule;

#[derive(Serialize, Deserialize, Eq, PartialEq, Clone)]
pub struct Member<'a> {
    #[serde(skip_serializing_if = "u64_zero")]
    #[serde(default)]
    pub flags: u64,

    #[serde(skip_serializing_if = "cow_str_is_empty")]
    #[serde(default)]
    pub name: Cow<'a, str>,
}

#[allow(unused)]
pub mod member_flag {
    pub const BRIDGING_ALLOWED: u64 = 0x0001;
}

#[derive(Serialize, Deserialize, Eq, PartialEq, Clone)]
pub struct Topology<'a> {
    pub timestamp: i64,

    #[serde(skip_serializing_if = "cow_str_is_empty")]
    #[serde(default)]
    pub name: Cow<'a, str>,

    #[serde(skip_serializing_if = "slice_is_empty")]
    #[serde(default)]
    pub rules: Cow<'a, [Rule]>,

    #[serde(skip_serializing_if = "FlatSortedMap::is_empty")]
    #[serde(default)]
    pub dns_resolvers: FlatSortedMap<'a, Cow<'a, str>, InetAddress>,

    #[serde(skip_serializing_if = "FlatSortedMap::is_empty")]
    #[serde(default)]
    pub dns_names: FlatSortedMap<'a, Cow<'a, str>, InetAddress>,

    #[serde(skip_serializing_if = "FlatSortedMap::is_empty")]
    #[serde(default)]
    pub members: FlatSortedMap<'a, Blob<IDENTITY_FINGERPRINT_SIZE>, Member<'a>>,
}

#[inline(always)]
fn u64_zero(i: &u64) -> bool {
    *i == 0
}

#[inline(always)]
fn cow_str_is_empty<'a>(s: &Cow<'a, str>) -> bool {
    s.is_empty()
}

#[inline(always)]
fn slice_is_empty<T, S: AsRef<[T]>>(x: &S) -> bool {
    x.as_ref().is_empty()
}
