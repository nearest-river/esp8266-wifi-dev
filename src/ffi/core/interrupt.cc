#include "../prelude.h"
#include "../os/task.h"
#include "../os/port.h"
#include "interrupt.h"
#include "../xtensa/instructions.h"
#include "misc.h"


// xPortSysTickHandle is defined in FreeRTOS/Source/portable/esp8266/port.c but
// does not exist in any header files.
void xPortSysTickHandle(void);

/* The following "functions" manipulate the stack at a low level and thus cannot be coded directly in C */

void IRAM vPortYield(void) {
  asm("wsr    a0, excsave1            \n\
       addi   sp, sp, -80             \n\
       s32i   a0, sp, 4               \n\
       addi   a0, sp, 80              \n\
       s32i   a0, sp, 16              \n\
       rsr    a0, ps                  \n\
       s32i   a0, sp, 8               \n\
       rsr    a0, excsave1            \n\
       s32i   a0, sp, 12              \n\
       movi   a0, _xt_user_exit       \n\
       s32i   a0, sp, 0               \n\
       call0  _xt_int_enter           \n\
       call0  vPortEnterCritical      \n\
       call0  vTaskSwitchContext      \n\
       call0  vPortExitCritical       \n\
       call0  _xt_int_exit            \n\
  ");
}

void IRAM _xt_int_enter(void) {
    asm("   s32i   a12, sp, 60             \n\
            s32i   a13, sp, 64             \n\
            mov    a12, a0                 \n\
            call0  _xt_context_save    \n\
            movi   a0, pxCurrentTCB        \n\
            l32i   a0, a0, 0               \n\
            s32i   sp, a0, 0               \n\
            mov    a0, a12                 \n\
    ");
}

void IRAM _xt_int_exit(void) {
    asm("   s32i   a14, sp, 68             \n\
            s32i   a15, sp, 72             \n\
            movi   sp, pxCurrentTCB        \n\
            l32i   sp, sp, 0               \n\
            l32i   sp, sp, 0               \n\
            movi   a14, pxCurrentTCB       \n\
            l32i   a14, a14, 0             \n\
            addi   a15, sp, 80             \n\
            s32i   a15, a14, 0             \n\
            call0  _xt_context_restore     \n\
            l32i   a14, sp, 68             \n\
            l32i   a15, sp, 72             \n\
            l32i   a0, sp, 0               \n\
    ");
}

void IRAM __xt_timer_int(void* arg) {
  u32 trigger_ccount;
  u32 current_ccount;
  u32 ccount_interval = portTICK_PERIOD_MS * os_get_cpu_frequency() * 1000;

  do {
    RSR(trigger_ccount, ccompare0);
    WSR(trigger_ccount + ccount_interval, ccompare0);
    ESYNC();
    xPortSysTickHandle();
    ESYNC();
    RSR(current_ccount, ccount);
  } while (current_ccount - trigger_ccount > ccount_interval);
}

void IRAM __xt_timer_int1(void) {
  vTaskSwitchContext();
}

#define INTENABLE_CCOMPARE  BIT(6)

void IRAM __xt_tick_timer_init(void) {
  u32 ints_enabled;
  u32 current_ccount;
  u32 ccount_interval = portTICK_PERIOD_MS * os_get_cpu_frequency() * 1000;

  RSR(current_ccount, ccount);
  WSR(current_ccount + ccount_interval, ccompare0);
  ints_enabled = 0;
  XSR(ints_enabled, intenable);
  WSR(ints_enabled | INTENABLE_CCOMPARE, intenable);
}
/*
void IRAM _xt_isr_unmask(u32 mask) {
  u32 ints_enabled;

  ints_enabled = 0;
  XSR(ints_enabled, intenable);
  WSR(ints_enabled | mask, intenable);
}*/

void IRAM _xt_isr_mask(u32 mask) {
  u32 ints_enabled;

  ints_enabled = 0;
  XSR(ints_enabled, intenable);
  WSR(ints_enabled & mask, intenable);
}

u32 IRAM _xt_read_ints(void) {
  u32 ints_enabled;

  RSR(ints_enabled, intenable);
  return ints_enabled;
}

void IRAM _xt_clear_ints(u32 mask) {
  WSR(mask, intclear);
}
