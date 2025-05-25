#include <string.h>
#include "prelude.h"
#include "wdev_regs.h"




/* Return a random 32-bit number.
 *
 * This is also used as a substitute for rand() called from
 * lmac.a:sdk_lmacTxFrame to avoid touching the newlib reent structures within
 * the NMI and the NMI code needs to be in IRAM.
 */
u32 IRAM hwrand(void) {
  return WDEV.HWRNG;
}

/* Fill a variable size buffer with data from the Hardware RNG */
void hwrand_fill(u8* buf,usize len) {
  for(usize i = 0; i < len; i+=4) {
    u32 random = WDEV.HWRNG;
    /* using memcpy here in case 'buf' is unaligned */
    memcpy(buf + i, &random, (i+4 <= len) ? 4 : (len % 4));
  }
}
