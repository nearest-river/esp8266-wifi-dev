#include "port.h"
#include "../core/interrupt.h"
#include "../prelude.h"
#include <stdio.h>
#include "task.h"
#include "../core/xtruntime.h"
#include "xtensa_context.h"
#include <malloc.h>
#include "queue.h"
#include <unistd.h>
#include "../xtensa/instructions.h"
#include "../xtensa/corebits.h"
#include "../newlib/syscalls.h"

#define sbrk(inc) _sbrk_r(_impure_ptr,inc)

unsigned cpu_sr;
u8 level1_int_disabled;
u8 NMIIrqIsOn;


/* Supervisor stack pointer entry. This is the "high water mark" of
   how far the supervisor stack grew down before task started. Is zero
   before the scheduler starts.

 After the scheduler starts, task stacks are all allocated from the
 heap and FreeRTOS checks for stack overflow.
*/
void *xPortSupervisorStackPointer;

void vAssertCalled(const char * pcFile, unsigned long ulLine) {
  printf("rtos assert %s %lu\n", pcFile, ulLine);
  abort();
  //for (;;);
}

/*
 * Stack initialization
 */
portSTACK_TYPE *pxPortInitialiseStack(portSTACK_TYPE* pxTopOfStack, TaskFunction pxCode, void *pvParameters ) {
  #define SET_STKREG(r,v) sp[(r) >> 2] = (portSTACK_TYPE)(v)
  portSTACK_TYPE* sp;
  portSTACK_TYPE* tp;

  /* Create interrupt stack frame aligned to 16 byte boundary */
  sp = (portSTACK_TYPE*) (((usize)(pxTopOfStack + 1) - XT_CP_SIZE - XT_STK_FRMSZ) & ~0xf);

  /* Clear the entire frame (do not use memset() because we don't depend on C library) */
  for (tp = sp; tp <= pxTopOfStack; ++tp)
      *tp = 0;

  /* Explicitly initialize certain saved registers */
  SET_STKREG( XT_STK_PC,      pxCode                        );  /* task entrypoint                  */
  SET_STKREG( XT_STK_A0,      0                           );  /* to terminate GDB backtrace       */
  SET_STKREG( XT_STK_A1,      (usize)sp + XT_STK_FRMSZ   );  /* physical top of stack frame      */
  SET_STKREG( XT_STK_A2,      pvParameters   );           /* parameters      */
  SET_STKREG( XT_STK_EXIT,    _xt_user_exit               );  /* user exception exit dispatcher   */

  /* Set initial PS to int level 0, EXCM disabled ('rfe' will enable), user mode. */
  SET_STKREG( XT_STK_PS,      PS_UM | PS_EXCM     );
  return sp;
}

static int PENDING_SOFT_SV;
static int PENDING_MACLAYER_SV;

void IRAM PendSV(SVCReqType req) {
  switch(req) {
  case SVCSoftware:
  vPortEnterCritical();
  PENDING_SOFT_SV=1;
  WSR(BIT(INUM_SOFT), interrupt);
  vPortExitCritical();
  break;
  case SVCMACLayer:
  PENDING_MACLAYER_SV=1;
  WSR(BIT(INUM_SOFT), interrupt);
  break;
  }
}


/* This MAC layer ISR handler is defined in libpp.a, and is called
 * after a Blob SV requests a soft interrupt by calling
 * PendSV(SVC_MACLayer).
 */
extern isize MacIsrSigPostDefHdl(void);

void IRAM SV_ISR(void *arg) {
  isize xHigherPriorityTaskWoken=false;
  if(PENDING_MACLAYER_SV) {
    xHigherPriorityTaskWoken=MacIsrSigPostDefHdl();
    PENDING_MACLAYER_SV=0;
  }
  if(xHigherPriorityTaskWoken || PENDING_SOFT_SV) {
    __xt_timer_int1();
    PENDING_SOFT_SV=0;
  }
}

void xPortSysTickHandle (void) {
  if(xTaskIncrementTick()) {
    vTaskSwitchContext();
  }
}

/*
 * See header file for description.
 */
isize xPortStartScheduler(void) {
  _xt_isr_attach(INUM_SOFT, SV_ISR, NULL);
  _xt_isr_unmask(BIT(INUM_SOFT));

  /* Initialize system tick timer interrupt and schedule the first tick. */
  _xt_isr_attach(INUM_TICK, __xt_timer_int, NULL);
  _xt_isr_unmask(BIT(INUM_TICK));
  __xt_tick_timer_init();

  vTaskSwitchContext();

  /* mark the supervisor stack pointer high water mark. xt_int_exit
     actually frees ~0x50 bytes off the stack, so this value is
     conservative.
  */
  __asm__ __volatile__ ("mov %0, a1\n" : "=a"(xPortSupervisorStackPointer));

  __xt_int_exit();

  /* Should not get here as the tasks are now running! */
  return true;
}

/* Determine free heap size via libc sbrk function & mallinfo

   sbrk gives total size in totally unallocated memory,
   mallinfo.fordblks gives free space inside area dedicated to heap.

   mallinfo is possibly non-portable, although glibc & newlib both support
   the fordblks member.
*/
usize xPortGetFreeHeapSize(void) {
  struct mallinfo mi = mallinfo();
  usize brk_val = (usize) sbrk(0);

  isize sp = (isize)xPortSupervisorStackPointer;
  if (sp == 0) {
      /* scheduler not started */
      SP(sp);
  }
  return sp - brk_val + mi.fordblks;
}

void vPortEndScheduler( void )
{
  /* No-op, nothing to return to */
}

/*-----------------------------------------------------------*/

static usize uxCriticalNesting = 0;

/* These nested vPortEnter/ExitCritical macros are called by SDK
 * libraries in libmain, libnet80211, libpp
 *
 * It may be possible to replace the global nesting count variable
 * with a save/restore of interrupt level, although it's difficult as
 * the functions have no return value.
 *
 * These should not be called from the NMI in regular operation and
 * the NMI must not touch the interrupt mask, but that might occur in
 * exceptional paths such as aborts and debug code.
 */
void IRAM vPortEnterCritical(void) {
  //portDISABLE_INTERRUPTS();
  uxCriticalNesting++;
}

/*-----------------------------------------------------------*/

void IRAM vPortExitCritical(void) {
  uxCriticalNesting--;
  if (uxCriticalNesting == 0) {
    //portENABLE_INTERRUPTS();
  }
}

/* Backward compatibility, for the sdk library. */

isize xTaskGenericCreate(TaskFunction pxTaskCode,
                                      const signed char * const pcName,
                                      unsigned short usStackDepth,
                                      void *pvParameters,
                                      usize uxPriority,
                                      TaskHandle* pxCreatedTask,
                                      portSTACK_TYPE *puxStackBuffer,
                                      const MemoryRegion* const xRegions) {
  (void)puxStackBuffer;
  (void)xRegions;
  return xTaskCreate(pxTaskCode, (const char * const)pcName, usStackDepth,
                     pvParameters, uxPriority, pxCreatedTask);
}

isize xQueueGenericReceive(Queue* xQueue, void * const pvBuffer,
                              TickType xTicksToWait, const isize xJustPeeking) {
  configASSERT(xJustPeeking == 0);
  return xQueueReceive(xQueue, pvBuffer, xTicksToWait);
}










