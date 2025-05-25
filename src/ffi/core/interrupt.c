#include "../prelude.h"
#include "../os/task.h"
#include "../os/port.h"
#include "interrupt.h"
#include "../xtensa/instructions.h"
#include "misc.h"

#define INTENABLE_CCOMPARE BIT(6)


// xPortSysTickHandle is defined in FreeRTOS/Source/portable/esp8266/port.c but
// does not exist in any header files.
void xPortSysTickHandle(void);

void IRAM __xt_int_enter(void) {
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

void IRAM __xt_int_exit(void) {
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

typedef struct XtIsrEntry {
  XtIsr handler;
  void* arg;
} XtIsrEntry;

XtIsrEntry isr[16];

bool esp_in_isr;

void IRAM _xt_isr_attach(u8 i,XtIsr func,void* arg) {
  isr[i].handler=func;
  isr[i].arg=arg;
}

/* Generic ISR handler.

   Handles all flags set for interrupts in 'intset'.
*/
u16 IRAM _xt_isr_handler(u16 intset) {
  esp_in_isr = true;

  /* WDT has highest priority (occasional WDT resets otherwise) */
  if(intset & BIT(INUM_WDT)) {
    _xt_clear_ints(BIT(INUM_WDT));
    isr[INUM_WDT].handler(NULL);
    intset -= BIT(INUM_WDT);
  }

  while(intset) {
    u8 index = __builtin_ffs(intset) - 1;
    u16 mask = BIT(index);
    _xt_clear_ints(mask);
    XtIsr handler = isr[index].handler;
    if(handler) {
      handler(isr[index].arg);
    }
    intset -= mask;
  }

  esp_in_isr = false;

  return 0;
}

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







