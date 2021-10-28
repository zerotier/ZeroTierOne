
#[repr(C)]
pub struct ZeroIDC {}

#[no_mangle]
pub extern "C" fn zeroidc_new() -> Box<ZeroIDC> {
    Box::new(ZeroIDC{})
}

#[no_mangle]
pub extern "C" fn zeroidc_delete(_: Option<Box<ZeroIDC>>) {}

#[no_mangle]
pub extern "C" fn zeroidc_start(idc: &mut ZeroIDC) {

}

#[no_mangle]
pub extern "C" fn zeroidc_stop(idc: &mut ZeroIDC) {

}

