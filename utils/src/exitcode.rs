// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

// These were taken from BSD sysexits.h to provide some standard for process exit codes.

pub const OK: i32 = 0;

pub const ERR_USAGE: i32 = 64;
pub const ERR_DATA_FORMAT: i32 = 65;
pub const ERR_NO_INPUT: i32 = 66;
pub const ERR_SERVICE_UNAVAILABLE: i32 = 69;
pub const ERR_INTERNAL: i32 = 70;
pub const ERR_OSERR: i32 = 71;
pub const ERR_OSFILE: i32 = 72;
pub const ERR_IOERR: i32 = 74;
pub const ERR_NOPERM: i32 = 77;
pub const ERR_CONFIG: i32 = 78;
