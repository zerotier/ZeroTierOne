// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::hash::Hash;
use std::sync::{Arc, Weak};

use crate::udp::BoundUdpSocket;

/// Local socket wrapper to provide to the core.
///
/// This implements very fast hash and equality in terms of an arbitrary unique ID assigned at
/// construction and holds a weak reference to the bound socket so dead sockets will silently
/// cease to exist or work. This also means that this code can check the weak count to determine
/// if the core is currently holding/using a socket for any reason.
#[derive(Clone)]
pub struct LocalSocket(pub Weak<BoundUdpSocket>, pub usize);

impl LocalSocket {
    /// Returns true if the wrapped socket appears to be in use by the core.
    #[inline(always)]
    pub fn in_use(&self) -> bool {
        self.0.weak_count() > 0
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
            s.address.to_string()
        } else {
            "(closed socket)".into()
        }
    }
}
