use core::arch::asm;

/*
/* _xt_user_exit is pushed onto the stack as part of the user exception handler,
   restores same set registers which were saved there and returns from exception */
_xt_user_exit:
        .global _xt_user_exit
        .type _xt_user_exit, @function
        l32i    a0, sp, 0x8
        wsr     a0, ps
        l32i    a0, sp, 0x4
        wsr     a0, epc1
        l32i    a0, sp, 0xc
        l32i    sp, sp, 0x10
        rsync
        rfe
*/

#[no_mangle]
#[inline(never)]
pub unsafe extern "C" fn _xt_user_exit()-> ! {
  asm!{
    "l32i    a0, sp, 0x8",
    "wsr     a0, ps",
    "l32i    a0, sp, 0x4",
    "wsr     a0, epc1",
    "l32i    a0, sp, 0xc",
    "l32i    sp, sp, 0x10",
    "rsync",
    "rfe",
    options(noreturn)
  };
}






