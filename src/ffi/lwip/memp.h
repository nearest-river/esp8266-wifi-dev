#ifndef LWIP_MEMP_H
#define LWIP_MEMP_H
#include "opt.h"
#include "lwipopts.h"
#include "../prelude.h"


typedef usize mem_ptr_t;

/* run once with empty definition to handle all custom includes in lwippools.h */
#define LWIP_MEMPOOL(name,num,size,desc)
#include "memp_std.h"

/** Create the list of all memory pools managed by memp. MEMP_MAX represents a NULL pool at the end */
typedef enum {
#define LWIP_MEMPOOL(name,num,size,desc)  MEMP_##name,
#include "memp_std.h"
  MEMP_MAX
} memp_t;

#if !MEMP_MEM_MALLOC || MEMP_OVERFLOW_CHECK
struct memp {
  struct memp *next;
#if MEMP_OVERFLOW_CHECK
  const char *file;
  int line;
#endif /* MEMP_OVERFLOW_CHECK */
};
#endif /* !MEMP_MEM_MALLOC || MEMP_OVERFLOW_CHECK */


/** Memory pool descriptor */
struct memp_desc {
#if defined(LWIP_DEBUG) || MEMP_OVERFLOW_CHECK || LWIP_STATS_DISPLAY
  const char* desc;
#endif /* LWIP_DEBUG || MEMP_OVERFLOW_CHECK || LWIP_STATS_DISPLAY */
#if MEMP_STATS
  struct stats_mem* stats;
#endif
  u16 size;
#if !MEMP_MEM_MALLOC
  u16 num;
  u8* base;
  struct memp** tab;
#endif /* MEMP_MEM_MALLOC */
};


#ifndef MEM_SANITY_REGION_BEFORE
#define MEM_SANITY_REGION_BEFORE  16
#endif // !MEM_SANITY_REGION_BEFORE

#ifndef MEM_SANITY_REGION_AFTER
#define MEM_SANITY_REGION_AFTER   16
#endif


#ifndef LWIP_MEM_ALIGN_SIZE
#define LWIP_MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1U) & ~(MEM_ALIGNMENT-1U))
#endif

#define MEM_SANITY_REGION_BEFORE_ALIGNED    LWIP_MEM_ALIGN_SIZE(MEM_SANITY_REGION_BEFORE)

#define MEMP_SIZE (LWIP_MEM_ALIGN_SIZE(sizeof(struct memp)) + MEM_SANITY_REGION_BEFORE_ALIGNED)

#define LWIP_MEM_ALIGN(addr) ((void*)(((mem_ptr_t)(addr) + MEM_ALIGNMENT - 1) & ~(mem_ptr_t)(MEM_ALIGNMENT-1)))

#define MEM_SANITY_REGION_AFTER_ALIGNED     LWIP_MEM_ALIGN_SIZE(MEM_SANITY_REGION_AFTER)

#define LWIP_CONST_CAST(target_type, val) ((target_type)((isize)val))

#define LWIP_ALIGNMENT_CAST(target_type, val) LWIP_CONST_CAST(target_type, val)


extern const struct memp_desc* const memp_pools[MEMP_MAX];


void memp_free(memp_t type,void* mem);




#endif // !LWIP_MEMP_H
