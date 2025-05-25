#ifndef NETBUF_H
#define NETBUF_H
#include "../prelude.h"
#include "prelude.h"
#include "err.h"


/** "Network buffer" - contains data and addressing info */
struct netbuf {
  struct pbuf *p, *ptr;
  ip_addr_t addr;
  u16 port;
#if LWIP_NETBUF_RECVINFO || LWIP_CHECKSUM_ON_COPY
  u8 flags;
  u16 toport_chksum;
#if LWIP_NETBUF_RECVINFO
  ip_addr_t toaddr;
#endif /* LWIP_NETBUF_RECVINFO */
#endif /* LWIP_NETBUF_RECVINFO || LWIP_CHECKSUM_ON_COPY */
};



#if LWIP_CHECKSUM_ON_COPY
#define netbuf_set_chksum(buf, chksum) do { (buf)->flags = NETBUF_FLAG_CHKSUM; \
                                            (buf)->toport_chksum = chksum; } while(0)
#endif /* LWIP_CHECKSUM_ON_COPY */

#define netbuf_len(buf)              ((buf)->p->tot_len)


void netbuf_free(struct netbuf *buf);
void netbuf_delete(struct netbuf* buf);
void* netbuf_alloc(struct netbuf* buf,u16 size);
err_t netbuf_ref(struct netbuf* buf,const void* dataptr,u16 size);




#endif // !NETBUF_H
