use crate::*;
use crate::bindings::capi as ztcore;
use serde::{Deserialize, Serialize};

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct Path {
    pub endpoint: Endpoint,
    pub lastSend: i64,
    pub lastReceive: i64,
    pub alive: bool,
    pub preferred: bool
}

impl Path {
    #[inline]
    pub(crate) fn new_from_capi(p: &ztcore::ZT_Path) -> Path {
        Path{
            endpoint: Endpoint::new_from_capi(&p.endpoint),
            lastSend: p.lastSend,
            lastReceive: p.lastReceive,
            alive: p.alive != 0,
            preferred: p.preferred != 0
        }
    }
}
