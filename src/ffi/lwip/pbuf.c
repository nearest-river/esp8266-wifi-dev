#include "pbuf.h"
#include "opt.h"
#include "prelude.h"
#include "priv_sockets.h"
#include "memp.h"
#include "mem.h"
#include "../os/port.h"

#define PERF_START    /* null definition */
#ifndef LWIP_SUPPORT_CUSTOM_PBUF
#define LWIP_SUPPORT_CUSTOM_PBUF ((IP_FRAG && !LWIP_NETIF_TX_SINGLE_PBUF) || (LWIP_IPV6 && LWIP_IPV6_FRAG))
#endif



/**
 * Keep account of the number the PP RX pool buffers being used in lwip,
 * to help make decision about the number of OOSEQ buffers to maintain etc.
 */
volatile u32 pp_rx_pool_usage;

void system_pp_recycle_rx_pkt(void*);

/* Support for recycling a pbuf from the sdk rx pool, and accounting for the
 * number of these used in lwip. */
void pp_recycle_rx_pbuf(struct pbuf* p) {
  LWIP_ASSERT("expected esf_buf", p->esf_buf);
  system_pp_recycle_rx_pkt(p->esf_buf);
  vPortEnterCritical();
  LWIP_ASSERT("pp_rx_pool_usage underflow", pp_rx_pool_usage > 0);
  pp_rx_pool_usage--;
  vPortExitCritical();
}

u8 pbuf_free(struct pbuf* p) {
  u8 alloc_src;
  struct pbuf* q;
  u8 count;

  if (p == NULL) {
    // LWIP_ASSERT("p != NULL", p != NULL);
    /* if assertions are disabled, proceed with debug output */
    /* LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                ("pbuf_free(p == NULL) was called.\n")); */
    return 0;
  }
  // LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_free(%p)\n", (void *)p));

  PERF_START;

  count = 0;
  while (p != NULL) {
    LWIP_PBUF_REF_T ref;
    // SYS_ARCH_DECL_PROTECT(old_level);
    sys_prot_t old_level=sys_arch_protect();

    // SYS_ARCH_PROTECT(old_level);
    // LWIP_ASSERT("pbuf_free: p->ref > 0", p->ref > 0);
    ref = --(p->ref);
    // SYS_ARCH_UNPROTECT(old_level);
    sys_arch_unprotect(old_level);
    if (ref == 0) {
      q = p->next;
      // LWIP_DEBUGF( PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_free: deallocating %p\n", (void *)p));
      alloc_src = pbuf_get_allocsrc(p);
#if LWIP_SUPPORT_CUSTOM_PBUF
      if ((p->flags & PBUF_FLAG_IS_CUSTOM) != 0) {
        struct pbuf_custom* pc = (struct pbuf_custom*)p;
        // LWIP_ASSERT("pc->custom_free_function != NULL", pc->custom_free_function != NULL);
        pc->custom_free_function(p);
      } else
#endif /* LWIP_SUPPORT_CUSTOM_PBUF */
      {
        /* is this a pbuf from the pool? */
        if (alloc_src == PBUF_TYPE_ALLOC_SRC_MASK_STD_MEMP_PBUF_POOL) {
          memp_free(MEMP_PBUF_POOL, p);
          /* is this a ROM or RAM referencing pbuf? */
        } else if (alloc_src == PBUF_TYPE_ALLOC_SRC_MASK_STD_MEMP_PBUF) {
          memp_free(MEMP_PBUF, p);
          /* type == PBUF_RAM */
        } else if (alloc_src == PBUF_TYPE_ALLOC_SRC_MASK_STD_HEAP) {
          mem_free(p);
#ifdef ESP_OPEN_RTOS
        } else if (alloc_src == PBUF_TYPE_ALLOC_SRC_MASK_ESP_RX) {
          pp_recycle_rx_pbuf(p);
          memp_free(MEMP_PBUF, p);
#endif
        } else {
          /* @todo: support freeing other types */
          LWIP_ASSERT("invalid pbuf type", 0);
        }
      }
      count++;
      /* proceed to next pbuf */
      p = q;
      /* p->ref > 0, this pbuf is still referenced to */
      /* (and so the remaining pbufs in chain as well) */
    } else {
      LWIP_DEBUGF( PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_free: %p has ref %"U16_F", ending here.\n", (void *)p, (u16_t)ref));
      /* stop walking through the chain */
      p = NULL;
    }
  }
  PERF_STOP("pbuf_free");
  /* return number of de-allocated pbufs */
  return count;
}


u16 pbuf_copy_partial(const struct pbuf* buf, void* dataptr, u16 len, u16 offset) {
  const struct pbuf* p;
  u16 left = 0;
  u16 buf_copy_len;
  u16 copied_total = 0;

  LWIP_ERROR("pbuf_copy_partial: invalid buf", (buf != NULL), return 0;);
  LWIP_ERROR("pbuf_copy_partial: invalid dataptr", (dataptr != NULL), return 0;);

  /* Note some systems use byte copy if dataptr or one of the pbuf payload pointers are unaligned. */
  for (p = buf; len != 0 && p != NULL; p = p->next) {
    if ((offset != 0) && (offset >= p->len)) {
      /* don't copy from this buffer -> on to the next */
      offset = (u16)(offset - p->len);
    } else {
      /* copy from this buffer. maybe only partially. */
      buf_copy_len = (u16)(p->len - offset);
      if (buf_copy_len > len) {
        buf_copy_len = len;
      }
      /* copy the necessary parts of the buffer */
      MEMCPY(&((char *)dataptr)[left], &((char *)p->payload)[offset], buf_copy_len);
      copied_total = (u16)(copied_total + buf_copy_len);
      left = (u16)(left + buf_copy_len);
      len = (u16)(len - buf_copy_len);
      offset = 0;
    }
  }
  return copied_total;
}


struct pbuf* pbuf_free_header(struct pbuf *q, u16 size) {
  struct pbuf *p = q;
  u16 free_left = size;
  while (free_left && p) {
    if (free_left >= p->len) {
      struct pbuf *f = p;
      free_left = (u16)(free_left - p->len);
      p = p->next;
      f->next = 0;
      pbuf_free(f);
    } else {
      pbuf_remove_header(p, free_left);
      free_left = 0;
    }
  }
  return p;
}


u8 pbuf_remove_header(struct pbuf* p,usize header_size_decrement) {
  void* payload;
  u16 increment_magnitude;

  LWIP_ASSERT("p != NULL", p != NULL);
  if ((p == NULL) || (header_size_decrement > 0xFFFF)) {
    return 1;
  }
  if (header_size_decrement == 0) {
    return 0;
  }

  increment_magnitude = (u16)header_size_decrement;
  /* Check that we aren't going to move off the end of the pbuf */
  LWIP_ERROR("increment_magnitude <= p->len", (increment_magnitude <= p->len), return 1;);

  /* remember current payload pointer */
  payload = p->payload;
  LWIP_UNUSED_ARG(payload); /* only used in LWIP_DEBUGF below */

  /* increase payload pointer (guarded by length check above) */
  p->payload = (u8*)p->payload + header_size_decrement;
  /* modify pbuf length fields */
  p->len = (u16)(p->len - increment_magnitude);
  p->tot_len = (u16)(p->tot_len - increment_magnitude);

  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("pbuf_remove_header: old %p new %p (%"U16_F")\n",
              (void *)payload, (void *)p->payload, increment_magnitude));

  return 0;
}







