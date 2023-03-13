use std::collections::HashMap;
use std::sync::{Arc, Mutex, RwLock};

use crate::protocol;
use crate::protocol::PacketBuffer;
use crate::vl1::*;

use zerotier_utils::buffer::OutOfBoundsError;
use zerotier_utils::sync::RMaybeWLockGuard;

pub struct ReplicationService {}

impl ReplicationService {
    pub fn new() -> Self {
        Self {}
    }
}
