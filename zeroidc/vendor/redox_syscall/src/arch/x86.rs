use core::{mem, slice};
use core::arch::asm;
use core::ops::{Deref, DerefMut};

use super::error::{Error, Result};

macro_rules! syscall {
    ($($name:ident($a:ident, $($b:ident, $($c:ident, $($d:ident, $($e:ident, $($f:ident, )?)?)?)?)?);)+) => {
        $(
            pub unsafe fn $name(mut $a: usize, $($b: usize, $($c: usize, $($d: usize, $($e: usize, $($f: usize)?)?)?)?)?) -> Result<usize> {
                asm!(
                    "int 0x80",
                    inout("eax") $a,
                    $(
                        in("ebx") $b,
                        $(
                            in("ecx") $c,
                            $(
                                in("edx") $d,
                                $(
                                    in("esi") $e,
                                    $(
                                        in("edi") $f,
                                    )?
                                )?
                            )?
                        )?
                    )?
                    options(nostack),
                );

                Error::demux($a)
            }
        )+
    };
}

syscall! {
    syscall0(a,);
    syscall1(a, b,);
    syscall2(a, b, c,);
    syscall3(a, b, c, d,);
    // Must be done custom because LLVM reserves ESI
    //syscall4(a, b, c, d, e,);
    //syscall5(a, b, c, d, e, f,);
}

pub unsafe fn syscall4(mut a: usize, b: usize, c: usize, d: usize, e: usize)
                       -> Result<usize> {
    asm!(
        "xchg esi, {e}
        int 0x80
        xchg esi, {e}",
        e = in(reg) e,
        inout("eax") a,
        in("ebx") b,
        in("ecx") c,
        in("edx") d,
        options(nostack),
    );

    Error::demux(a)
}

pub unsafe fn syscall5(mut a: usize, b: usize, c: usize, d: usize, e: usize, f: usize)
                       -> Result<usize> {
    asm!(
        "xchg esi, {e}
        int 0x80
        xchg esi, {e}",
        e = in(reg) e,
        inout("eax") a,
        in("ebx") b,
        in("ecx") c,
        in("edx") d,
        in("edi") f,
        options(nostack),
    );

    Error::demux(a)
}

#[derive(Clone, Copy, Debug, Default)]
#[repr(packed)]
pub struct EnvRegisters {
    pub fsbase: u32,
    pub gsbase: u32,
}

impl Deref for EnvRegisters {
    type Target = [u8];
    fn deref(&self) -> &[u8] {
        unsafe {
            slice::from_raw_parts(self as *const EnvRegisters as *const u8, mem::size_of::<EnvRegisters>())
        }
    }
}

impl DerefMut for EnvRegisters {
    fn deref_mut(&mut self) -> &mut [u8] {
        unsafe {
            slice::from_raw_parts_mut(self as *mut EnvRegisters as *mut u8, mem::size_of::<EnvRegisters>())
        }
    }
}
