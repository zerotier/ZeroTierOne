//! Minimum supported Rust version: 1.28

use std::num::NonZeroUsize;

#[cfg_attr(any(target_os = "linux", target_os = "android"), path = "linux.rs")]
#[cfg_attr(target_os = "freebsd", path = "freebsd.rs")]
#[cfg_attr(any(target_os = "macos", target_os = "ios"), path = "apple.rs")]
mod imp;

/// Obtain the number of threads currently part of the active process. Returns `None` if the number
/// of threads cannot be determined.
pub fn num_threads() -> Option<NonZeroUsize> {
    imp::num_threads()
}

/// Determine if the current process is single-threaded. Returns `None` if the number of threads
/// cannot be determined.
pub fn is_single_threaded() -> Option<bool> {
    num_threads().map(|n| n.get() == 1)
}

#[cfg(test)]
mod test {
    use std::num::NonZeroUsize;

    // Run each expression in its own thread.
    macro_rules! threaded {
        ($first:expr;) => {
            $first;
        };
        ($first:expr; $($rest:expr;)*) => {
            $first;
            ::std::thread::spawn(|| {
                threaded!($($rest;)*);
            })
            .join()
            .unwrap();
        };
    }

    #[test]
    fn num_threads() {
        threaded! {
            assert_eq!(super::num_threads().map(NonZeroUsize::get), Some(1));
            assert_eq!(super::num_threads().map(NonZeroUsize::get), Some(2));
            assert_eq!(super::num_threads().map(NonZeroUsize::get), Some(3));
            assert_eq!(super::num_threads().map(NonZeroUsize::get), Some(4));
            assert_eq!(super::num_threads().map(NonZeroUsize::get), Some(5));
            assert_eq!(super::num_threads().map(NonZeroUsize::get), Some(6));
        }
    }

    #[test]
    fn is_single_threaded() {
        threaded! {
            assert_eq!(super::is_single_threaded(), Some(true));
            assert_eq!(super::is_single_threaded(), Some(false));
            assert_eq!(super::is_single_threaded(), Some(false));
            assert_eq!(super::is_single_threaded(), Some(false));
            assert_eq!(super::is_single_threaded(), Some(false));
            assert_eq!(super::is_single_threaded(), Some(false));
        }
    }
}
