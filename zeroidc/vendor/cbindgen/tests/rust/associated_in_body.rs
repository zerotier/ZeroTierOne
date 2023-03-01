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

#[no_mangle]
pub extern "C" fn root(flags: AlignFlags) {}
