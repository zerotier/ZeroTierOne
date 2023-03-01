#[no_mangle]
pub extern fn loop_forever() -> ! {
    loop {}
}

#[no_mangle]
pub extern fn normal_return() -> u8 {
    0
}
