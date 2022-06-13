bitflags! {
    /// Constants shared by multiple CSS Box Alignment properties
    ///
    /// These constants match Gecko's `NS_STYLE_ALIGN_*` constants.
    #[derive(MallocSizeOf, ToComputedValue)]
    #[repr(C)]
    pub struct AlignFlags: u8 {
        /// 'auto'
        const AUTO = 0;
        /// 'normal'
        const NORMAL = 1;
        /// 'start'
        const START = 1 << 1;
        /// 'end'
        const END = 1 << 2;
        /// 'flex-start'
        const FLEX_START = 1 << 3;
    }
}

bitflags! {
    #[repr(C)]
    pub struct DebugFlags: u32 {
        /// Flag with the topmost bit set of the u32
        const BIGGEST_ALLOWED = 1 << 31;
    }
}

#[no_mangle]
pub extern "C" fn root(flags: AlignFlags, bigger_flags: DebugFlags) {}
