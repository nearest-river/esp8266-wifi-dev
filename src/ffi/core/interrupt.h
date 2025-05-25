#ifndef OS_INTERRUPT_H
#define OS_INTERRUPT_H
#include "../prelude.h"
#include "xtruntime.h"


// xt_isr_num_t
typedef enum {
  INUM_WDEV_FIQ = 0,
  INUM_SLC = 1,
  INUM_SPI = 2,
  INUM_RTC = 3,
  INUM_GPIO = 4,
  INUM_UART = 5,
  INUM_TICK = 6, /* RTOS timer tick, possibly xtensa CPU CCOMPARE0(?) */
  INUM_SOFT = 7,
  INUM_WDT = 8,
  INUM_TIMER_FRC1 = 9,

  /* FRC2 default handler. Configured by sdk_ets_timer_init, which
     runs as part of default libmain.a startup code, assigns
     interrupt handler to sdk_vApplicationTickHook+0x68
   */
  INUM_TIMER_FRC2 = 10,
} InterruptNum;



void __xt_int_exit(void);
void _xt_user_exit(void);
void __xt_tick_timer_init(void);
void __xt_timer_int(void *);
void __xt_timer_int1(void);

/* The normal running level is 0.
 * The system tick isr, timer frc2_isr, sv_isr etc  run at level 1.
 * Debug exceptions run at level 2?
 * The wdev nmi runs at level 3.
 */
static inline u32 _xt_get_intlevel(void) {
  u32 level;
  __asm__ volatile("rsr %0, ps" : "=a"(level));
  return level & 0xf;
}

/*
 * There are conflicting definitions for XCHAL_EXCM_LEVEL. Newlib
 * defines it to be 1 and xtensa_rtos.h defines it to be 3. Don't want
 * 3 as that is for the NMI and might want to check that the OS apis
 * are not entered in level 3. Setting the interrupt level to 3 does
 * not disable the NMI anyway. So set the level to 2.
 */

#ifdef XCHAL_EXCM_LEVEL
#undef XCHAL_EXCM_LEVEL
#define XCHAL_EXCM_LEVEL 2
#endif

/* Disable interrupts and return the old ps value, to pass into
   _xt_restore_interrupts later.

   This is desirable to use in place of
   portDISABLE_INTERRUPTS/portENABLE_INTERRUPTS for
   non-FreeRTOS & non-portable code.
*/
static inline u32 _xt_disable_interrupts(void) {
  u32 old_level;
  __asm__ volatile ("rsil %0, " XTSTR(XCHAL_EXCM_LEVEL) : "=a" (old_level));
  return old_level;
}

/* Restore PS level. Intended to be used with _xt_disable_interrupts */
static inline void _xt_restore_interrupts(u32 new_ps) {
  __asm__ volatile ("wsr %0, ps; rsync" :: "a" (new_ps));
}

void IRAM _xt_isr_unmask(u32 unmask);
void IRAM _xt_isr_mask(u32 mask);
u32 IRAM _xt_read_ints(void);
void IRAM _xt_clear_ints(u32 mask);

typedef void (*XtIsr)(void* arg);
void _xt_isr_attach (u8 i,XtIsr func,void* arg);


#endif // !OS_INTERRUPT_H
