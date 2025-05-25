#ifndef OS_PORT_H
#define OS_PORT_H
#include "../prelude.h"
#include "config.h"


typedef u32 TickType;
#define portMAX_DELAY ((TickType)0xffffffffUL)
#define portSTACK_GROWTH ((isize)-1)
#define portBYTE_ALIGNMENT 8
#define portUSING_MPU_WRAPPERS 0
#define portPRIVILEGE_BIT ((usize)0)
#define portSETUP_TCB( pxTCB ) ( void ) pxTCB
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY	(-1)
#define portSTACK_TYPE u32
#define portTICK_PERIOD_MS			((TickType) 1000 / configTICK_RATE_HZ)


typedef long BaseType;
typedef unsigned long UBaseType;

typedef enum {
  SVCSoftware=1,
  SVCMACLayer=2,
} SVCReqType;





extern u8 NMIIrqIsOn;

void IRAM vPortEnterCritical(void);
void IRAM vPortExitCritical(void);
void IRAM PendSV(SVCReqType req);


#define portYIELD()	PendSV(SVCSoftware)
#define portYIELD_WITHIN_API portYIELD

#ifndef configASSERT
#define configASSERT(x)
#endif

// TODO(nate)
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() {}
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portASSERT_IF_INTERRUPT_PRIORITY_INVALID() configASSERT(!sdk_NMIIrqIsOn)

#ifndef portTICK_TYPE_IS_ATOMIC
#define portTICK_TYPE_IS_ATOMIC 0
#endif // !portTICK_TYPE_IS_ATOMIC


#define portSET_INTERRUPT_MASK_FROM_ISR() 0
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedStatusValue ) ( void ) uxSavedStatusValue


#if !portTICK_TYPE_IS_ATOMIC
	/* Either variables of tick type cannot be read atomically, or
	portTICK_TYPE_IS_ATOMIC was not set - map the critical sections used when
	the tick count is returned to the standard critical section macros. */
	#define portTICK_TYPE_ENTER_CRITICAL() vPortEnterCritical()
	#define portTICK_TYPE_EXIT_CRITICAL() vPortExitCritical()
	#define portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()
	#define portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( x ) portCLEAR_INTERRUPT_MASK_FROM_ISR( ( x ) )
#else
	/* The tick type can be read atomically, so critical sections used when the
	tick count is returned can be defined away. */
	#define portTICK_TYPE_ENTER_CRITICAL()
	#define portTICK_TYPE_EXIT_CRITICAL()
	#define portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR() 0
	#define portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( x ) ( void ) x
#endif


#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )
#ifndef portALLOCATE_SECURE_CONTEXT
	#define portALLOCATE_SECURE_CONTEXT( ulSecureStackSize )
#endif


#ifndef traceTASK_PRIORITY_INHERIT
	/* Called when a task attempts to take a mutex that is already held by a
	lower priority task.  pxTCBOfMutexHolder is a pointer to the TCB of the task
	that holds the mutex.  uxInheritedPriority is the priority the mutex holder
	will inherit (the priority of the task that is attempting to obtain the
	muted. */
	#define traceTASK_PRIORITY_INHERIT( pxTCBOfMutexHolder, uxInheritedPriority )
#endif


#ifndef traceTASK_PRIORITY_DISINHERIT
	/* Called when a task releases a mutex, the holding of which had resulted in
	the task inheriting the priority of a higher priority task.
	pxTCBOfMutexHolder is a pointer to the TCB of the task that is releasing the
	mutex.  uxOriginalPriority is the task's configured (base) priority. */
	#define traceTASK_PRIORITY_DISINHERIT( pxTCBOfMutexHolder, uxOriginalPriority )
#endif




#endif // !OS_PORT
