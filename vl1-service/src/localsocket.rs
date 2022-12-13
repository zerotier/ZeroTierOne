// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

use std::hash::Hash;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Weak};

use crate::sys::udp::BoundUdpSocket;

static LOCAL_SOCKET_UNIQUE_ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

/// Local socket wrapper to provide to the core.
///
/// This implements very fast hash and equality in terms of an arbitrary unique ID assigned at
/// construction and holds a weak reference to the bound socket so dead sockets will silently
/// cease to exist or work. This also means that this code can check the weak count to determine
/// if the core is currently holding/using a socket for any reason.
#[derive(Clone)]
pub struct LocalSocket(pub(crate) Weak<BoundUdpSocket>, usize);

impl LocalSocket {
    pub fn new(s: &Arc<BoundUdpSocket>) -> Self {
        Self(Arc::downgrade(s), LOCAL_SOCKET_UNIQUE_ID_COUNTER.fetch_add(1, Ordering::SeqCst))
    }

    #[inline(always)]
    pub fn is_valid(&self) -> bool {
        self.0.strong_count() > 0
    }

    #[inline(always)]
    pub fn socket(&self) -> Option<Arc<BoundUdpSocket>> {
        self.0.upgrade()
    }
}

impl PartialEq for LocalSocket {
    #[inline(always)]
    fn eq(&self, other: &Self) -> bool {
        self.1 == other.1
    }
}

impl Eq for LocalSocket {}

impl Hash for LocalSocket {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.1.hash(state)
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
