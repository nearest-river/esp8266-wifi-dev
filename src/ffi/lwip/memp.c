#include "memp.h"
#include "prelude.h"
#include "priv_sockets.h"
#include "mem.h"


void mem_overflow_check_raw(void*,usize);

#if MEMP_OVERFLOW_CHECK
static void memp_overflow_check_element(struct memp* p, const struct memp_desc* desc) {
  mem_overflow_check_raw((u8*)p + MEMP_SIZE, desc->size);
}
#endif

#if MEMP_OVERFLOW_CHECK>=2
/**
 * Do an overflow check for all elements in every pool.
 *
 * @see memp_overflow_check_element for a description of the check
 */
static void memp_overflow_check_all(void) {
  u16 i, j;
  struct memp* p;
  sys_prot_t old_level=sys_arch_protect();

  for(i = 0; i < MEMP_MAX; ++i) {
    p = (struct memp *)LWIP_MEM_ALIGN(memp_pools[i]->base);
    for (j = 0; j < memp_pools[i]->num; ++j) {
      memp_overflow_check_element(p, memp_pools[i]);
      p = LWIP_ALIGNMENT_CAST(struct memp *, ((u8 *)p + MEMP_SIZE + memp_pools[i]->size + MEM_SANITY_REGION_AFTER_ALIGNED));
    }
  }
  sys_arch_unprotect(old_level);
}
#endif /* MEMP_OVERFLOW_CHECK >= 2 */


static void do_memp_free_pool(const struct memp_desc* desc,void* mem) {
  /* cast through void* to get rid of alignment warnings */
  struct memp* memp=(struct memp*)(void*)((u8*)mem - MEMP_SIZE);
  sys_prot_t old_level=sys_arch_protect();

  /* LWIP_ASSERT("memp_free: mem properly aligned",
              ((mem_ptr_t)mem % MEM_ALIGNMENT) == 0); */

#if MEMP_OVERFLOW_CHECK == 1
  memp_overflow_check_element(memp, desc);
#endif /* MEMP_OVERFLOW_CHECK */

#if MEMP_STATS
  desc->stats->used--;
#endif

#if MEMP_MEM_MALLOC
  LWIP_UNUSED_ARG(desc);
  sys_arch_unprotect(old_level);
  mem_free(memp);
#else /* MEMP_MEM_MALLOC */
  memp->next=*desc->tab;
  *desc->tab=memp;

#if MEMP_SANITY_CHECK
  // LWIP_ASSERT("memp sanity", memp_sanity(desc));
#endif /* MEMP_SANITY_CHECK */

  sys_arch_unprotect(old_level);
#endif /* !MEMP_MEM_MALLOC */
}


void memp_free(memp_t type,void* mem) {
#ifdef LWIP_HOOK_MEMP_AVAILABLE
  struct memp* old_first;
#endif

  // LWIP_ERROR("memp_free: type < MEMP_MAX", (type < MEMP_MAX), return;);

  if (mem == NULL) {
    return;
  }

#if MEMP_OVERFLOW_CHECK >= 2
  memp_overflow_check_all();
#endif /* MEMP_OVERFLOW_CHECK >= 2 */

#ifdef LWIP_HOOK_MEMP_AVAILABLE
  old_first=*memp_pools[type]->tab;
#endif

  do_memp_free_pool(memp_pools[type], mem);

#ifdef LWIP_HOOK_MEMP_AVAILABLE
  if (old_first == NULL) {
    // LWIP_HOOK_MEMP_AVAILABLE(type);
  }
#endif
}




