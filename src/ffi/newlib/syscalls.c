#include "syscalls.h"
#include "../lock.h"
#include "../os/port.h"
#include "../uart.h"
#include "../priv_err.h"
#include "../lwip/sockets.h"
#include <sys/reent.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include "../os/semaphore.h"
#include "../xtensa/instructions.h"
#include "../hwrand.h"



/*
 * The file descriptor index space is allocated in blocks. The first block of 3
 * is for newlib I/O the stdin stdout and stderr. The next block of
 * MEMP_NUM_NETCONN is allocated for lwip sockets, and the remainer to file
 * system descriptors. The newlib default FD_SETSIZE is 64.
 */
#if LWIP_SOCKET_OFFSET < 3
#error Expecting a LWIP_SOCKET_OFFSET >= 3, to allow room for the standard I/O descriptors.
#endif
#define FILE_DESCRIPTOR_OFFSET (LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN)
#if FILE_DESCRIPTOR_OFFSET > FD_SETSIZE
#error Too many lwip sockets for the FD_SETSIZE.
#endif

void* xPortSupervisorStackPointer=(void*)0x40000000;

IRAM
void* _sbrk_r(struct _reent* r, int incr) {
  extern u8 _heap_start; /* linker script defined */
  static u8* heap_end;
  u8* prev_heap_end;

  if(heap_end == NULL) {
	heap_end = &_heap_start;
  }
  prev_heap_end = heap_end;

  isize sp = (isize)xPortSupervisorStackPointer;
  if(sp == 0) {// scheduler not started
    SP(sp);
  }

  if((isize)heap_end + incr >= sp) {
    r->_errno = ENOMEM;
    return (u8*)-1;
  }

  heap_end += incr;

  return (u8*) prev_heap_end;
}


/* Insert a disjoint region into the nano malloc pool. Create a malloc chunk,
 * filling the size as newlib nano malloc expects, and then free it. */
void nano_malloc_insert_chunk(void *start, usize size) {
  *(u32*)start = size;
  free(start + sizeof(usize));
}


/* syscall implementation for stdio write to UART */
__attribute__((weak))
isize _write_stdout_r(struct _reent* r, int fd, const void *ptr, usize len ) {
  for(int i = 0; i < len; i++) {
    /* Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour) */
    if(((u8 *)ptr)[i] == '\r')
      continue;
    if(((u8 *)ptr)[i] == '\n')
      uart_putc(0, '\r');
    uart_putc(0, ((u8 *)ptr)[i]);
  }
  return len;
}

static _WriteFunction* current_stdout_write_r = (_WriteFunction*)&_write_stdout_r;

void set_write_stdout(_WriteFunction *f) {
  if  (f != NULL) {
    current_stdout_write_r = f;
  } else {
    current_stdout_write_r = (_WriteFunction*)&_write_stdout_r;
  }
}

_WriteFunction* get_write_stdout() {
  return current_stdout_write_r;
}

/* default implementation, replace in a filesystem */
__attribute__((weak))
isize _write_filesystem_r(struct _reent* r, int fd, const void *ptr, usize len) {
  r->_errno = EBADF;
  return -1;
}



__attribute__((weak))
isize _write_r(struct _reent* r, int fd, const void *ptr, usize len) {
  if(fd >= FILE_DESCRIPTOR_OFFSET) {
    return _write_filesystem_r(r, fd, ptr, len);
  }
  if(fd >= LWIP_SOCKET_OFFSET) {
    return lwip_write(fd, ptr, len);
  }
  if(fd == r->_stdout->_file) {
    return current_stdout_write_r(r, fd, ptr, len);
  }
  r->_errno = EBADF;
  return -1;
}


/* syscall implementation for stdio read from UART */
__attribute__((weak))
isize _read_stdin_r(struct _reent* r, int fd, void *ptr, usize len) {
  int ch, i;
  uart_rxfifo_wait(0, 1);
  for(i = 0; i < len; i++) {
    ch = uart_getc_nowait(0);
    if(ch < 0) break;
    ((u8 *)ptr)[i] = ch;
  }
  return i;
}

/* default implementation, replace in a filesystem */
__attribute__((weak))
isize _read_filesystem_r( struct _reent* r, int fd, void *ptr, usize len ) {
  r->_errno = EBADF;
  return -1;
}


__attribute__((weak))
isize _read_r( struct _reent* r, int fd, void *ptr, usize len ) {
  if(fd >= FILE_DESCRIPTOR_OFFSET) {
    return _read_filesystem_r(r, fd, ptr, len);
  }
  if(fd >= LWIP_SOCKET_OFFSET) {
    return lwip_read(fd, ptr, len);
  }
  if(fd == r->_stdin->_file) {
    return _read_stdin_r(r, fd, ptr, len);
  }
  r->_errno = EBADF;
  return -1;
}

/* default implementation, replace in a filesystem */
__attribute__((weak))
int _close_filesystem_r(struct _reent* r, int fd) {
  r->_errno = EBADF;
  return -1;
}


__attribute__((weak))
int _close_r(struct _reent* r, int fd) {
  if(fd >= FILE_DESCRIPTOR_OFFSET) {
    return _close_filesystem_r(r, fd);
  }
  if(fd >= LWIP_SOCKET_OFFSET) {
    return lwip_close(fd);
  }
  r->_errno = EBADF;
  return -1;
}


/* Stub syscall implementations follow, to allow compiling newlib functions that
   pull these in via various codepaths
*/
__attribute__((weak, alias("syscall_returns_enosys"))) 
int _open_r(struct _reent *r, const char *pathname, int flags, int mode);

__attribute__((weak, alias("syscall_returns_enosys"))) 
int _unlink_r(struct _reent *r, const char *path);

__attribute__((weak, alias("syscall_returns_enosys"))) 
int _fstat_r(struct _reent *r, int fd, struct stat *buf);

__attribute__((weak, alias("syscall_returns_enosys"))) 
int _stat_r(struct _reent *r, const char *pathname, struct stat *buf);

__attribute__((weak, alias("syscall_returns_enosys"))) 
off_t _lseek_r(struct _reent *r, int fd, off_t offset, int whence);

__attribute__((weak, alias("_gettimeofday_r")))
int _gettimeofday_r (struct _reent *ptr, struct timeval *ptimeval, void *ptimezone) {
  ptimeval->tv_sec = 0;
  ptimeval->tv_usec = 0;
  errno = ENOSYS;
  return -1;
}

/* Generic stub for any newlib syscall that fails with errno ENOSYS
   ("Function not implemented") and a return value equivalent to
   (int)-1. */
static int syscall_returns_enosys(struct _reent* r) {
  r->_errno=ENOSYS;
  return -1;
}

int getentropy(void *ptr, usize n) {
  hwrand_fill(ptr, n);
  return 0;
}

void _arc4random_getentropy_fail(void) {
}

void _exit(int status) {
  while(1);
}

/*
 * Newlib lock implementation. Some newlib locks are statically allocated, but
 * can not be statically initialized so are set to NULL and initialized at
 * startup. The malloc lock is used before it can be initialized so there are
 * runtime checks on the functions that use it early.
 */
static int locks_initialized = 0;

_lock_t __arc4random_mutex;
_lock_t __at_quick_exit_mutex;
//extern _lock_t __dd_hash_mutex;
_lock_t __tz_mutex;

_lock_t __atexit_recursive_mutex;
_lock_t __env_recursive_mutex;
_lock_t __malloc_recursive_mutex;
_lock_t __sfp_recursive_mutex;
_lock_t __sinit_recursive_mutex;




void init_newlib_locks() {
#if 0
  /* Used a separate mutex for each lock.
   * Each mutex uses about 96 bytes which adds up. */
  _lock_init(&__arc4random_mutex);
  _lock_init(&__at_quick_exit_mutex);
  //_lock_init(&__dd_hash_mutex);
  _lock_init(&__tz_mutex);

  _lock_init_recursive(&__atexit_recursive_mutex);
  _lock_init_recursive(&__env_recursive_mutex);
  _lock_init_recursive(&__malloc_recursive_mutex);
  _lock_init_recursive(&__sfp_recursive_mutex);
  _lock_init_recursive(&__sinit_recursive_mutex);
#else
  /* Reuse one mutex and one recursive mutex for this set, reducing memory
   * usage. Newlib will still allocate other locks dynamically and some of
   * those need to be separate such as the file lock where a thread might
   * block with them held. */
  _lock_init(&__arc4random_mutex);
  __at_quick_exit_mutex = __arc4random_mutex;
  //__dd_hash_mutex = __arc4random_mutex;
  __tz_mutex = __arc4random_mutex;

  _lock_init_recursive(&__atexit_recursive_mutex);
  __env_recursive_mutex  = __atexit_recursive_mutex;
  __malloc_recursive_mutex = __atexit_recursive_mutex;
  __sfp_recursive_mutex = __atexit_recursive_mutex;
  __sinit_recursive_mutex = __atexit_recursive_mutex;
#endif

  locks_initialized = 1;
}

void _lock_init(_lock_t *lock) {
  *lock = (_lock_t)xSemaphoreCreateMutex();
}

void _lock_init_recursive(_lock_t *lock) {
  *lock = (_lock_t)xSemaphoreCreateRecursiveMutex();
}

void _lock_close(_lock_t *lock) {
  vSemaphoreDelete((Queue*)*lock);
  *lock = 0;
}

void _lock_close_recursive(_lock_t *lock) {
  vSemaphoreDelete((Queue*)*lock);
  *lock = 0;
}

void _lock_acquire(_lock_t *lock) {
  xSemaphoreTake((Queue*)*lock, portMAX_DELAY);
}

void _lock_acquire_recursive(_lock_t *lock) {
  if(locks_initialized) {
    if(NMIIrqIsOn) {
      uart_putc(0, ':');
      return;
    }
    xSemaphoreTakeRecursive((Queue*)*lock, portMAX_DELAY);
  }
}

int _lock_try_acquire(_lock_t* lock) {
  return xSemaphoreTake((Queue*)*lock, 0);
}

int _lock_try_acquire_recursive(_lock_t *lock) {
  return xSemaphoreTakeRecursive((Queue*)*lock, 0);
}

void _lock_release(_lock_t *lock) {
  xSemaphoreGive((Queue*)*lock);
}

void _lock_release_recursive(_lock_t *lock) {
  if(locks_initialized) {
    if(NMIIrqIsOn) {
      return;
    }
    xSemaphoreGiveRecursive((Queue*)*lock);
  }
}







