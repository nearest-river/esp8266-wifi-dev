#include "netbuf.h"
#include "pbuf.h"
#include "memp.h"


void netbuf_free(struct netbuf* buf) {
  // LWIP_ERROR("netbuf_free: invalid buf", (buf != NULL), return;);
  if (buf->p != NULL) {
    pbuf_free(buf->p);
  }

  buf->p = buf->ptr = NULL;
#if LWIP_CHECKSUM_ON_COPY
  buf->flags = 0;
  buf->toport_chksum = 0;
#endif /* LWIP_CHECKSUM_ON_COPY */
}


/**
 * @ingroup netbuf
 * Deallocate a netbuf allocated by netbuf_new().
 *
 * @param buf pointer to a netbuf allocated by netbuf_new()
 */
void netbuf_delete(struct netbuf* buf) {
  if (buf != NULL) {
    if (buf->p != NULL) {
      pbuf_free(buf->p);
      buf->p = buf->ptr = NULL;
    }
    memp_free(MEMP_NETBUF, buf);
  }
}



void* netbuf_alloc(struct netbuf* buf,u16 size) {
  LWIP_ERROR("netbuf_alloc: invalid buf", (buf != NULL), return NULL;);

  /* Deallocate any previously allocated memory. */
  if (buf->p != NULL) {
    pbuf_free(buf->p);
  }
  buf->p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
  if (buf->p == NULL) {
    return NULL;
  }
  LWIP_ASSERT("check that first pbuf can hold size",
              (buf->p->len >= size));
  buf->ptr = buf->p;
  return buf->p->payload;
}



err_t netbuf_ref(struct netbuf* buf,const void* dataptr,u16 size) {
  LWIP_ERROR("netbuf_ref: invalid buf", (buf != NULL), return ERR_ARG;);
  if (buf->p != NULL) {
    pbuf_free(buf->p);
  }
  buf->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);
  if (buf->p == NULL) {
    buf->ptr = NULL;
    return ERR_MEM;
  }
  ((struct pbuf_rom *)buf->p)->payload = dataptr;
  buf->p->len = buf->p->tot_len = size;
  buf->ptr = buf->p;
  return ERR_OK;
}
















