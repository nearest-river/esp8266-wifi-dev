#ifndef OS_CONFIG_H
#define OS_CONFIG_H
#include "../prelude.h"


#define configUSE_QUEUE_SETS 0
#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1

#define configUSE_PREEMPTION 1
// true for wifi stuff
#define configUSE_TRACE_FACILITY 0

#define configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 0
#define configUSE_MUTEXES 1

#define configUSE_16_BIT_TICKS 0
#if configUSE_16_BIT_TICKS
	#define pdINTEGRITY_CHECK_VALUE 0x5a5a
#else
	#define pdINTEGRITY_CHECK_VALUE 0x5a5a5a5aUL
#endif

#define configUSE_TIMERS 1
// FIXME(nate)
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_RECURSIVE_MUTEXES 1


#define configQUEUE_REGISTRY_SIZE 0
#define configUSE_CO_ROUTINES 0
#define configMAX_PRIORITIES 0xf


#define configCHECK_FOR_STACK_OVERFLOW 2

#define configMAX_TASK_NAME_LEN 16
#define configINITIAL_TICK_COUNT 0
#define configMINIMAL_STACK_SIZE 128
#define configMINIMAL_SECURE_STACK_SIZE configMINIMAL_STACK_SIZE

#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 100
#endif




#define mtCOVERAGE_TEST_MARKER()






#endif // !OS_CONFIG_H
