// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::hash::Hash;
use std::sync::{Arc, Weak};

use crate::sys::udp::BoundUdpSocket;

/// Local socket wrapper to provide to the core.
///
/// This wraps a bound UDP socket in weak form so sockets that are released by the UDP
/// binding engine can be "garbage collected" by the core.
#[repr(transparent)]
#[derive(Clone)]
pub struct LocalSocket(Weak<BoundUdpSocket>);

impl LocalSocket {
    #[inline]
    pub fn new(s: &Arc<BoundUdpSocket>) -> Self {
        Self(Arc::downgrade(s))
    }

    #[inline]
    pub fn is_valid(&self) -> bool {
        self.0.strong_count() > 0
    }

    #[inline]
    pub fn socket(&self) -> Option<Arc<BoundUdpSocket>> {
        self.0.upgrade()
    }
}

impl PartialEq for LocalSocket {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.0.ptr_eq(&other.0)
    }
}

impl Eq for LocalSocket {}

impl Hash for LocalSocket {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.0.as_ptr().hash(state)
    }
}

impl ToString for LocalSocket {
    fn to_string(&self) -> String {
        if let Some(s) = self.0.upgrade() {
            s.bind_address.to_string()
        } else {
            "(closed socket)".into()
        }
    }
}
