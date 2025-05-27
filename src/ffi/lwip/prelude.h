#ifndef LWIP_TYPES_H
#define LWIP_TYPES_H
#include "../prelude.h"


typedef int sys_sem_t;
typedef unsigned int nfds_t;
typedef u8 sys_mbox_t;
typedef int sys_prot_t;
typedef u8 sys_mutex_t;
typedef u32 sys_thread_t;




#ifndef LWIP_PLATFORM_ASSERT
#define LWIP_PLATFORM_ASSERT(x) do {printf("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); fflush(NULL); abort();} while(0)
#include <stdio.h>
#include <stdlib.h>
#endif

#ifndef PERF_STOP
#define PERF_STOP(x) /* null definition */
#endif // !PERF_STOP




#endif // !LWIP_TYPES_H
