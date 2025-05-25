#include "../prelude.h"
#include "mallinfo.h"
#include <string.h>

#ifndef MALLOC_DIRECTION
#define MALLOC_DIRECTION 1
#endif


typedef struct FreeListEntry {
  usize size;
  struct FreeListEntry *next;
} FreeListEntry;

extern void* __malloc_end;
extern FreeListEntry* __malloc_freelist;

/* Return the number of bytes that need to be added to X to make it
   aligned to an ALIGN boundary.  ALIGN must be a power of 2.  */
#define M_ALIGN(x, align) (-(usize)(x) & ((align) - 1))

/* Return the number of bytes that need to be subtracted from X to make it
   aligned to an ALIGN boundary.  ALIGN must be a power of 2.  */
#define M_ALIGN_SUB(x, align) ((usize)(x) & ((align) - 1))

extern void __malloc_start;

/* This is the minimum gap allowed between __malloc_end and the top of
   the stack.  This is only checked for when __malloc_end is
   decreased; if instead the stack grows into the heap, silent data
   corruption will result.  */
#define MALLOC_MINIMUM_GAP 32

#ifdef __xstormy16__
register void* stack_pointer asm ("r15");
#define MALLOC_LIMIT stack_pointer
#else
#define MALLOC_LIMIT __builtin_frame_address(0)
#endif

#if MALLOC_DIRECTION < 0
#define CAN_ALLOC_P(required)				\
  (((usize) __malloc_end - (usize)MALLOC_LIMIT	\
    - MALLOC_MINIMUM_GAP) >= (required))
#else
#define CAN_ALLOC_P(required)				\
  (((usize)MALLOC_LIMIT - (usize) __malloc_end	\
    - MALLOC_MINIMUM_GAP) >= (required))
#endif

/* real_size is the size we actually have to allocate, allowing for
   overhead and alignment.  */
#define REAL_SIZE(sz)						\
  ((sz) < sizeof (FreeListEntry) - sizeof (usize)	\
   ? sizeof (FreeListEntry)				\
   : sz + sizeof (usize) + M_ALIGN(sz, sizeof (usize)))




MallInfo mallinfo(void) {
  MallInfo r;
  FreeListEntry* fr;
  usize free_size;
  usize total_size;
  usize free_blocks;

  memset(&r,0,sizeof(r));

  free_size=0;
  free_blocks=0;
  for(fr=__malloc_freelist;fr;fr=fr->next) {
    free_size+=fr->size;
    free_blocks++;

    if(!fr->next) {
      int atend;
      if(MALLOC_DIRECTION > 0) {
        atend = (void *)((usize)fr + fr->size) == __malloc_end;
      } else {
        atend = (void *)fr == __malloc_end;
      }
      if(atend) {
        r.keepcost = fr->size;
      }
    }
  }

  if(MALLOC_DIRECTION > 0) {
    total_size=(char*)__malloc_end - (char*)&__malloc_start;
  } else {
    total_size=(char*)&__malloc_start - (char*)__malloc_end;
  }

  // cuss DEBUG
  // #ifdef DEBUG
  // /* Fixme: should walk through all the in-use blocks and see if
  // they're valid.  */
  // #endif

  r.arena=total_size;
  r.fordblks=free_size;
  r.uordblks=total_size-free_size;
  r.ordblks=free_blocks;
  return r;
}

