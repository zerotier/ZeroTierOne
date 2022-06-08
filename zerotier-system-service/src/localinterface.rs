// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::hash::Hash;

/// Lightweight container for local system network interface names/IDs.
///
/// On *nix systems this will be an interface like 'eth0' stuffed into a u128. On Windows
/// this will be a network device GUID, which is also 128-bit. This will need to be revised
/// if there are OSes out there that use interface names or IDs longer than 16 bytes. The
/// point here is to have something tiny and cheap for the core to store internally.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct LocalInterface(u128);

impl LocalInterface {
    #[cfg(unix)]
    pub fn from_unix_interface_name(name: &str) -> Self {
        let mut tmp = [0_u8; 16];
        let nb = name.as_bytes();
        let l = nb.len();
        assert!(l <= 16); // do any *nix OSes have device names longer than 16 bytes?
        tmp[..l].copy_from_slice(&nb[..l]);
        Self(u128::from_be_bytes(tmp))
    }
}

impl ToString for LocalInterface {
    #[cfg(unix)]
    fn to_string(&self) -> String {
        let b = self.0.to_be_bytes();
        let mut l = 0;
        for bb in b.iter() {
            if *bb > 0 {
                l += 1;
            } else {
                break;
            }
        }
        String::from_utf8_lossy(&b[..l]).to_string()
    }

    #[cfg(windows)]
    fn to_string(&self) -> String {
        zerotier_core_crypto::hex::to_string(&self.0.to_be_bytes())
    }
}
