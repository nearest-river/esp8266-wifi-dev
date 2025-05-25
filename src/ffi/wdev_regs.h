#ifndef WDEV_REGS_H
#define WDEV_REGS_H
#include "prelude.h"

// #include "common_macros.h"


/* Note: This memory region is not currently well understood.  Pretty much all
 * of the definitions here are from reverse-engineering the Espressif SDK code,
 * many are just educated guesses, and almost certainly some are misleading or
 * wrong.  If you can improve on any of this, please contribute!
 */

struct WDEV_REGS {
  u32 volatile _unknown0[768];  // 0x0000 - 0x0bfc
  u32 volatile SYS_TIME;        // 0x0c00
  u32 volatile _unknown1[144];  // 0x0c04 - 0x0e40
  u32 volatile HWRNG;           // 0xe44 HW RNG, see http://esp8266-re.foogod.com/wiki/Random_Number_Generator
} __attribute__ (( packed ));

#define WDEV_BASE 0x3FF20000
#define WDEV (*(struct WDEV_REGS*)(WDEV_BASE))


// _Static_assert(sizeof(struct WDEV_REGS) == 0xe48, "WDEV_REGS is the wrong size");

/* Extra paranoid check about the HWRNG address, as if this becomes
   wrong there will be no obvious symptoms apart from a lack of
   entropy.
*/
// _Static_assert(&WDEV.HWRNG == (void*)0x3FF20E44, "HWRNG register is at wrong address");




#endif /* WDEV_REGS_H */
