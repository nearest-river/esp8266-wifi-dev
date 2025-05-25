#ifndef NEWLIB_SYSCALL_H
#define NEWLIB_SYSCALL_H

#include "../prelude.h"
#include "../os/queue.h"


#define LWIP_SOCKET_OFFSET 3
#define MEMP_NUM_NETCONN 12

typedef isize _WriteFunction(struct _reent* r, int fd, const void* ptr,usize len);
extern struct _reent* _impure_ptr;

void* IRAM _sbrk_r(struct _reent* r, int incr);





#endif
