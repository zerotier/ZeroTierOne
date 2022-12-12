// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod arrayvec;
pub mod blob;
pub mod buffer;
pub mod defer;
pub mod dictionary;
pub mod error;
#[allow(unused)]
pub mod exitcode;
pub mod gate;
pub mod gatherarray;
pub mod hex;
pub mod io;
pub mod json;
pub mod marshalable;
pub mod memory;
pub mod pool;
#[cfg(feature = "tokio")]
pub mod reaper;
pub mod ringbuffer;
pub mod ringbuffermap;
pub mod sync;
pub mod thing;
pub mod varint;

#[cfg(feature = "tokio")]
pub use tokio;

#[cfg(feature = "tokio")]
pub use futures_util;

/// Initial value that should be used for monotonic tick time variables.
pub const NEVER_HAPPENED_TICKS: i64 = 0;

const BASE64_URL_SAFE_NO_PAD_ENGINE: base64::engine::fast_portable::FastPortable =
    base64::engine::fast_portable::FastPortable::from(&base64::alphabet::URL_SAFE, base64::engine::fast_portable::NO_PAD);

/// Encode base64 using URL-safe alphabet and no padding.
pub fn base64_encode_url_nopad(bytes: &[u8]) -> String {
    base64::encode_engine(bytes, &BASE64_URL_SAFE_NO_PAD_ENGINE)
}

/// Decode base64 using URL-safe alphabet and no padding, or None on error.
pub fn base64_decode_url_nopad(b64: &str) -> Option<Vec<u8>> {
    base64::decode_engine(b64, &BASE64_URL_SAFE_NO_PAD_ENGINE).ok()
}

/// Get milliseconds since unix epoch.
#[inline]
pub fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_millis() as i64
}

/// Get milliseconds since an arbitrary time in the past, guaranteed to monotonically increase.
#[inline]
pub fn ms_monotonic() -> i64 {
    static STARTUP_INSTANT: std::sync::RwLock<Option<std::time::Instant>> = std::sync::RwLock::new(None);
    let si = *STARTUP_INSTANT.read().unwrap();
    if let Some(si) = si {
        si.elapsed().as_millis() as i64
    } else {
        STARTUP_INSTANT
            .write()
            .unwrap()
            .get_or_insert(std::time::Instant::now())
            .elapsed()
            .as_millis() as i64
    }
}

/// Wait for a kill signal (e.g. SIGINT or OS-equivalent) sent to this process and return when received.
#[cfg(unix)]
pub fn wait_for_process_abort() {
    if let Ok(mut signals) = signal_hook::iterator::Signals::new(&[libc::SIGINT, libc::SIGTERM, libc::SIGQUIT]) {
        'wait_for_exit: loop {
            for signal in signals.wait() {
                match signal as libc::c_int {
                    libc::SIGINT | libc::SIGTERM | libc::SIGQUIT => {
                        break 'wait_for_exit;
                    }
                    _ => {}
                }
            }
            std::thread::sleep(std::time::Duration::from_millis(100));
        }
    } else {
        panic!("unable to listen for OS signals");
    }
}

#[cold]
#[inline(never)]
pub extern "C" fn unlikely_branch() {}

#[cfg(test)]
mod tests {
    use super::ms_monotonic;
    use std::time::Duration;

    #[test]
    fn monotonic_clock_sanity_check() {
        let start = ms_monotonic();
        std::thread::sleep(Duration::from_millis(500));
        let end = ms_monotonic();
        // per docs:
        //
        // The thread may sleep longer than the duration specified due to scheduling specifics or
        // platform-dependent functionality. It will never sleep less.
        //
        assert!((end - start).abs() >= 500);
        assert!((end - start).abs() < 750);
    }
}
