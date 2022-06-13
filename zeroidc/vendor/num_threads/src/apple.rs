extern crate libc;

use std::num::NonZeroUsize;

use self::libc::{kern_return_t, mach_msg_type_number_t, mach_port_t, thread_t};

// This constant is from
// /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/
// usr/include/mach/machine/thread_state.h.
//
// It has not been updated since Apple devices started to support 64-bit ARM (iOS), so it
// should be very stable.
const THREAD_STATE_MAX: i32 = 1296;
#[allow(non_camel_case_types)]
// https://github.com/apple/darwin-xnu/blob/a1babec6b135d1f35b2590a1990af3c5c5393479/osfmk/mach/mach_types.defs#L155
type task_inspect_t = mach_port_t;
#[allow(non_camel_case_types)]
// https://github.com/apple/darwin-xnu/blob/a1babec6b135d1f35b2590a1990af3c5c5393479/osfmk/mach/mach_types.defs#L238
type thread_array_t = [thread_t; THREAD_STATE_MAX as usize];

extern "C" {
    // https://developer.apple.com/documentation/kernel/1537751-task_threads/
    fn task_threads(
        target_task: task_inspect_t,
        act_list: *mut thread_array_t,
        act_listCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
}

pub(crate) fn num_threads() -> Option<NonZeroUsize> {
    // http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/task_threads.html
    let mut thread_state = [0u32; THREAD_STATE_MAX as usize];
    let mut thread_count = 0;

    // Safety: `mach_task_self` always returns a valid value, `thread_state` is large enough, and
    // both it and `thread_count` are writable.
    let result =
        unsafe { task_threads(libc::mach_task_self(), &mut thread_state, &mut thread_count) };

    if result == libc::KERN_SUCCESS {
        NonZeroUsize::new(thread_count as usize)
    } else {
        None
    }
}
