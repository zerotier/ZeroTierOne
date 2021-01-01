use crate::*;
use crate::bindings::capi as ztcore;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
pub struct Path {
    pub endpoint: Endpoint,
    #[serde(rename = "lastSend")]
    pub last_send: i64,
    #[serde(rename = "lastReceive")]
    pub last_receive: i64,
    pub alive: bool,
    pub preferred: bool
}

impl Path {
    #[inline]
    pub(crate) fn new_from_capi(p: &ztcore::ZT_Path) -> Path {
        Path{
            endpoint: Endpoint::new_from_capi(&p.endpoint),
            last_send: p.lastSend,
            last_receive: p.lastReceive,
            alive: p.alive != 0,
            preferred: p.preferred != 0
        }
    }
}
