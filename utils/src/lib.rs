// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

pub mod arrayvec;
pub mod blob;
pub mod buffer;
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
pub mod ringbuffer;
pub mod ringbuffermap;
pub mod varint;

#[cfg(feature = "tokio")]
pub mod reaper;

#[cfg(feature = "tokio")]
pub use tokio;

/// A monotonic ticks value for "never happened" that should be lower than any initial value.
pub const NEVER_HAPPENED_TICKS: i64 = i64::MIN / 2;

/// Get milliseconds since unix epoch.
pub fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_millis() as i64
}

/// Get milliseconds since an arbitrary time in the past, guaranteed to monotonically increase.
pub fn ms_monotonic() -> i64 {
    static STARTUP_INSTANT: parking_lot::RwLock<Option<std::time::Instant>> = parking_lot::RwLock::new(None);
    let si = *STARTUP_INSTANT.read();
    let instant_zero = if let Some(si) = si {
        si
    } else {
        *STARTUP_INSTANT.write().get_or_insert(std::time::Instant::now())
    };
    std::time::Instant::now().duration_since(instant_zero).as_millis() as i64
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
