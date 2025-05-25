#include "prelude.h"
#include "ip_addr.h"
#include "../os/port.h"
#include "../prelude.h"
#include "priv_sockets.h"
#include "err.h"
#include "sockets.h"

static struct lwip_sock sockets[NUM_SOCKETS];



isize lwip_write(int s,const void* data,usize size) {
  return lwip_send(s, data, size, 0);
}


isize lwip_send(int s,const void* data,usize size,int flags) {
  struct lwip_sock* sock;
  i8 err;
  u8 write_flags;
  usize written;

  sock=get_socket(s);
  if(!sock) {
    return -1;
  }

  if(NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_TCP) {
#if (LWIP_UDP || LWIP_RAW)
    done_socket(sock);
    return lwip_sendto(s, data, size, flags, NULL, 0);
#else /* (LWIP_UDP || LWIP_RAW) */
    sock_set_errno(sock, err_to_errno(ERR_ARG));
    done_socket(sock);
    return -1;
#endif /* (LWIP_UDP || LWIP_RAW) */
  }

  write_flags = (u8)(NETCONN_COPY |
                       ((flags & MSG_MORE)     ? NETCONN_MORE      : 0) |
                       ((flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0));
  written = 0;
  err = netconn_write_partly(sock->conn, data, size, write_flags, &written);

  sock_set_errno(sock, err_to_errno(err));
  done_socket(sock);
  return (err == ERR_OK ? (isize)written : -1);
}


#if LWIP_NETCONN_FULLDUPLEX
/* Thread-safe increment of sock->fd_used, with overflow check */
static void sock_inc_used(struct lwip_sock* sock) {
  LWIP_ASSERT("sock != NULL", sock != NULL);
  sys_prot_t lev=sys_arch_protect();
  ++sock->fd_used;
  LWIP_ASSERT("sock->fd_used != 0", sock->fd_used != 0);
  sys_arch_unprotect(lev);
}

/* Like sock_inc_used(), but called under SYS_ARCH_PROTECT lock. */
static void sock_inc_used_locked(struct lwip_sock* sock) {
  LWIP_ASSERT("sock != NULL", sock != NULL);

  ++sock->fd_used;
  LWIP_ASSERT("sock->fd_used != 0", sock->fd_used != 0);
}

/* In full-duplex mode,sock->fd_used != 0 prevents a socket descriptor from being
 * released (and possibly reused) when used from more than one thread
 * (e.g. read-while-write or close-while-write, etc)
 * This function is called at the end of functions using (try)get_socket*().
 */
static void done_socket_locked(struct lwip_sock* sock) {
  LWIP_ASSERT("sock != NULL", sock != NULL);

  LWIP_ASSERT("sock->fd_used > 0", sock->fd_used > 0);
  if (--sock->fd_used == 0) {
    if (sock->fd_free_pending) {
      /* free the socket */
      sock->fd_used = 1;
      free_socket(sock, sock->fd_free_pending & LWIP_SOCK_FD_FREE_TCP);
    }
  }
}

static void done_socket(struct lwip_sock* sock) {
  sys_prot_t lev=sys_arch_protect();
  done_socket_locked(sock);
  sys_arch_unprotect(lev);
}
#else /* LWIP_NETCONN_FULLDUPLEX */
#define sock_inc_used(sock)
#define sock_inc_used_locked(sock)
#define done_socket(sock)
#define done_socket_locked(sock)
#endif /* LWIP_NETCONN_FULLDUPLEX */

/* Translate a socket 'int' into a pointer (only fails if the index is invalid) */
static struct lwip_sock* tryget_socket_unconn_nouse(int fd) {
  int s = fd - LWIP_SOCKET_OFFSET;
  if ((s < 0) || (s >= NUM_SOCKETS)) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("tryget_socket_unconn(%d): invalid\n", fd));
    return NULL;
  }
  return &sockets[s];
}

struct lwip_sock* lwip_socket_dbg_get_socket(int fd) {
  return tryget_socket_unconn_nouse(fd);
}

/* Translate a socket 'int' into a pointer (only fails if the index is invalid) */
static struct lwip_sock* tryget_socket_unconn(int fd) {
  struct lwip_sock* ret = tryget_socket_unconn_nouse(fd);
  if (ret != NULL) {
    sock_inc_used(ret);
  }
  return ret;
}

/* Like tryget_socket_unconn(), but called under SYS_ARCH_PROTECT lock. */
static struct lwip_sock* tryget_socket_unconn_locked(int fd) {
  struct lwip_sock* ret = tryget_socket_unconn_nouse(fd);
  if (ret != NULL) {
    sock_inc_used_locked(ret);
  }
  return ret;
}

/**
 * Same as get_socket but doesn't set errno
 *
 * @param fd externally used socket index
 * @return struct lwip_sock for the socket or NULL if not found
 */
static struct lwip_sock* tryget_socket(int fd) {
  struct lwip_sock* sock = tryget_socket_unconn(fd);
  if (sock != NULL) {
    if (sock->conn) {
      return sock;
    }
    done_socket(sock);
  }
  return NULL;
}

/**
 * Map a externally used socket index to the internal socket representation.
 *
 * @param fd externally used socket index
 * @return struct lwip_sock for the socket or NULL if not found
 */
static struct lwip_sock* 
get_socket(int fd)
{
  struct lwip_sock* sock = tryget_socket(fd);
  if (!sock) {
    if ((fd < LWIP_SOCKET_OFFSET) || (fd >= (LWIP_SOCKET_OFFSET + NUM_SOCKETS))) {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("get_socket(%d): invalid\n", fd));
    }
    set_errno(EBADF);
    return NULL;
  }
  return sock;
}

/**
 * Allocate a new socket for a given netconn.
 *
 * @param newconn the netconn for which to allocate a socket
 * @param accepted 1 if socket has been created by accept(),
 *                 0 if socket has been created by socket()
 * @return the index of the new socket; -1 on error
 */
static int alloc_socket(struct netconn *newconn, int accepted) {
  int i;
  sys_prot_t lev;
  LWIP_UNUSED_ARG(accepted);

  /* allocate a new socket identifier */
  for (i = 0; i < NUM_SOCKETS; ++i) {
    /* Protect socket array */
    lev=sys_arch_protect();
    if (!sockets[i].conn) {
#if LWIP_NETCONN_FULLDUPLEX
      if (sockets[i].fd_used) {
        sys_arch_unprotect(lev);
        continue;
      }
      sockets[i].fd_used    = 1;
      sockets[i].fd_free_pending = 0;
#endif
      sockets[i].conn       = newconn;
      /* The socket is not yet known to anyone, so no need to protect
         after having marked it as used. */
      sys_arch_unprotect(lev);
      sockets[i].lastdata.pbuf = NULL;
#if LWIP_SOCKET_SELECT
      LWIP_ASSERT("sockets[i].select_waiting == 0", sockets[i].select_waiting == 0);
      sockets[i].rcvevent   = 0;
      /* TCP sendbuf is empty, but the socket is not yet writable until connected
       * (unless it has been created by accept()). */
      sockets[i].sendevent  = (NETCONNTYPE_GROUP(newconn->type) == NETCONN_TCP ? (accepted != 0) : 1);
      sockets[i].errevent   = 0;
#endif /* LWIP_SOCKET_SELECT */
      return i + LWIP_SOCKET_OFFSET;
    }
    sys_arch_unprotect(lev);
  }
  return -1;
}

/** Free a socket. The socket's netconn must have been
 * delete before!
 *
 * @param sock the socket to free
 * @param is_tcp != 0 for TCP sockets, used to free lastdata
 */
static void free_socket(struct lwip_sock* sock, int is_tcp) {
  union lwip_sock_lastdata lastdata;
  /* Protect socket array */
  sys_prot_t lev=sys_arch_protect();

#if LWIP_NETCONN_FULLDUPLEX
  LWIP_ASSERT("sock->fd_used > 0", sock->fd_used > 0);
  sock->fd_used--;
  if (sock->fd_used > 0) {
    sock->fd_free_pending = LWIP_SOCK_FD_FREE_FREE | (is_tcp ? LWIP_SOCK_FD_FREE_TCP : 0);
    sys_arch_unprotect(lev);
    return;
  }
#endif

  lastdata = sock->lastdata;
  sock->lastdata.pbuf = NULL;
  sock->conn = NULL;
  sys_arch_unprotect(lev);
  /* don't use 'sock' after this line, as another task might have allocated it */

  if (lastdata.pbuf != NULL) {
    if (is_tcp) {
      pbuf_free(lastdata.pbuf);
    } else {
      netbuf_delete(lastdata.netbuf);
    }
  }
}










static sys_prot_t critical_nesting;
sys_prot_t sys_arch_protect(void) {
  sys_prot_t prev;
  vPortEnterCritical();
  prev = critical_nesting;
  critical_nesting++;
  return prev;
}


void sys_arch_unprotect(sys_prot_t pval) {
  //(void) xValue;
  critical_nesting--;
  LWIP_ASSERT("unexpected critical_nestion", pval == critical_nesting);
  if (pval != critical_nesting) {
    // printf("lwip nesting %d\n", critical_nesting);
  }
  vPortExitCritical();
}


isize lwip_sendto(int s,const void* data,usize size,int flags, const struct sockaddr *to,socklen_t tolen) {
  struct lwip_sock* sock;
  err_t err;
  u16 short_size;
  u16 remote_port;
  struct netbuf buf;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP) {
#if LWIP_TCP
    done_socket(sock);
    return lwip_send(s, data, size, flags);
#else /* LWIP_TCP */
    LWIP_UNUSED_ARG(flags);
    sock_set_errno(sock, err_to_errno(ERR_ARG));
    done_socket(sock);
    return -1;
#endif /* LWIP_TCP */
  }

  if (size > MIN(0xFFFF, ISIZE_MAX)) {
    /* cannot fit into one datagram (at least for us) */
    sock_set_errno(sock, EMSGSIZE);
    done_socket(sock);
    return -1;
  }
  short_size = (u16)size;
/*  LWIP_ERROR("lwip_sendto: invalid address", (((to == NULL) && (tolen == 0)) ||
             (IS_SOCK_ADDR_LEN_VALID(tolen) &&
              ((to != NULL) && (IS_SOCK_ADDR_TYPE_VALID(to) && IS_SOCK_ADDR_ALIGNED(to))))),
             sock_set_errno(sock, err_to_errno(ERR_ARG)); done_socket(sock); return -1;);*/
  LWIP_UNUSED_ARG(tolen);

  buf.p = buf.ptr = NULL;
#if LWIP_CHECKSUM_ON_COPY
  buf.flags = 0;
#endif /* LWIP_CHECKSUM_ON_COPY */
  if(to) {
    SOCKADDR_TO_IPADDR_PORT(to, &buf.addr, remote_port);
  } else {
    remote_port = 0;
    ip_addr_set_any(NETCONNTYPE_ISIPV6(netconn_type(sock->conn)), &buf.addr);
  }
  buf.port=remote_port; //netbuf_fromport(&buf) = remote_port;


  /* LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_sendto(%d, data=%p, short_size=%"U16_F", flags=0x%x to=",
                              s, data, short_size, flags)); */
  // ip_addr_debug_print_val(SOCKETS_DEBUG, buf.addr);
  /* LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F"\n", remote_port)); */

#if LWIP_NETIF_TX_SINGLE_PBUF
  if(netbuf_alloc(&buf, short_size) == NULL) {
    err = ERR_MEM;
  } else {
#if LWIP_CHECKSUM_ON_COPY
    if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_RAW) {
      u16 chksum = LWIP_CHKSUM_COPY(buf.p->payload, data, short_size);
      netbuf_set_chksum(&buf, chksum);
    } else
#endif /* LWIP_CHECKSUM_ON_COPY */
    {
      MEMCPY(buf.p->payload, data, short_size);
    }
    err = ERR_OK;
  }
#else /* LWIP_NETIF_TX_SINGLE_PBUF */
  err = netbuf_ref(&buf, data, short_size);
#endif /* LWIP_NETIF_TX_SINGLE_PBUF */
  if (err == ERR_OK) {
#if LWIP_IPV4 && LWIP_IPV6
    /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
    if (IP_IS_V6_VAL(buf.addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&buf.addr))) {
      unmap_ipv4_mapped_ipv6(ip_2_ip4(&buf.addr), ip_2_ip6(&buf.addr));
      IP_SET_TYPE_VAL(buf.addr, IPADDR_TYPE_V4);
    }
#endif /* LWIP_IPV4 && LWIP_IPV6 */
    err = netconn_send(sock->conn, &buf);
  }

  netbuf_free(&buf);

  sock_set_errno(sock, err_to_errno(err));
  done_socket(sock);
  return (err == ERR_OK ? short_size : -1);
}

#if LWIP_IPV4 && LWIP_IPV6
static void sockaddr_to_ipaddr_port(const struct sockaddr* sockaddr, ip_addr_t* ipaddr,u16* port) {
  if ((sockaddr->sa_family) == AF_INET6) {
    SOCKADDR6_TO_IP6ADDR_PORT((const struct sockaddr_in6*)(const void*)(sockaddr), ipaddr, *port);
    ipaddr->type = IPADDR_TYPE_V6;
  } else {
    SOCKADDR4_TO_IP4ADDR_PORT((const struct sockaddr_in*)(const void*)(sockaddr), ipaddr, *port);
    ipaddr->type = IPADDR_TYPE_V4;
  }
}
#endif /* LWIP_IPV4 && LWIP_IPV6 */



isize lwip_read(int s,void* mem,usize len) {
  return lwip_recvfrom(s, mem, len, 0, NULL, NULL);
}


#if LWIP_TCP
/* Helper function to loop over receiving pbufs from netconn
 * until "len" bytes are received or we're otherwise done.
 * Keeps sock->lastdata for peeking or partly copying.
 */
static isize lwip_recv_tcp(struct lwip_sock* sock,void* mem,usize len,int flags) {
  u8 apiflags = NETCONN_NOAUTORCVD;
  isize recvd = 0;
  isize recv_left = (len <= ISIZE_MAX) ? (isize)len : ISIZE_MAX;

  LWIP_ASSERT("no socket given", sock != NULL);
  LWIP_ASSERT("this should be checked internally", NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP);

  if (flags & MSG_DONTWAIT) {
    apiflags |= NETCONN_DONTBLOCK;
  }

  do {
    struct pbuf* p;
    err_t err;
    u16 copylen;

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: top while sock->lastdata=%p\n", (void *)sock->lastdata.pbuf));
    /* Check if there is data left from the last recv operation. */
    if (sock->lastdata.pbuf) {
      p = sock->lastdata.pbuf;
    } else {
      /* No data was left from the previous operation, so we try to get
         some from the network. */
      err = netconn_recv_tcp_pbuf_flags(sock->conn, &p, apiflags);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: netconn_recv err=%d, pbuf=%p\n",
                                  err, (void *)p));

      if (err != ERR_OK) {
        if (recvd > 0) {
          /* already received data, return that (this trusts in getting the same error from
             netconn layer again next time netconn_recv is called) */
          goto lwip_recv_tcp_done;
        }
        /* We should really do some error checking here. */
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: p == NULL, error is \"%s\"!\n",
                                    lwip_strerr(err)));
        sock_set_errno(sock, err_to_errno(err));
        if (err == ERR_CLSD) {
          return 0;
        } else {
          return -1;
        }
      }
      LWIP_ASSERT("p != NULL", p != NULL);
      sock->lastdata.pbuf = p;
    }

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: buflen=%"U16_F" recv_left=%d off=%d\n",
                                p->tot_len, (int)recv_left, (int)recvd));

    if (recv_left > p->tot_len) {
      copylen = p->tot_len;
    } else {
      copylen = (u16)recv_left;
    }
    if (recvd + copylen < recvd) {
      /* overflow */
      copylen = (u16)(ISIZE_MAX - recvd);
    }

    /* copy the contents of the received buffer into
    the supplied memory pointer mem */
    pbuf_copy_partial(p, (u8 *)mem + recvd, copylen, 0);

    recvd += copylen;

    /* TCP combines multiple pbufs for one recv */
    LWIP_ASSERT("invalid copylen, len would underflow", recv_left >= copylen);
    recv_left -= copylen;

    /* Unless we peek the incoming message... */
    if ((flags & MSG_PEEK) == 0) {
      /* ... check if there is data left in the pbuf */
      LWIP_ASSERT("invalid copylen", p->tot_len >= copylen);
      if (p->tot_len - copylen > 0) {
        /* If so, it should be saved in the sock structure for the next recv call.
           We store the pbuf but hide/free the consumed data: */
        sock->lastdata.pbuf = pbuf_free_header(p, copylen);
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: lastdata now pbuf=%p\n", (void *)sock->lastdata.pbuf));
      } else {
        sock->lastdata.pbuf = NULL;
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: deleting pbuf=%p\n", (void *)p));
        pbuf_free(p);
      }
    }
    /* once we have some data to return, only add more if we don't need to wait */
    apiflags |= NETCONN_DONTBLOCK | NETCONN_NOFIN;
    /* @todo: do we need to support peeking more than one pbuf? */
  } while ((recv_left > 0) && !(flags & MSG_PEEK));
lwip_recv_tcp_done:
  if ((recvd > 0) && !(flags & MSG_PEEK)) {
    /* ensure window update after copying all data */
    netconn_tcp_recvd(sock->conn, (usize)recvd);
  }
  sock_set_errno(sock, 0);
  return recvd;
}
#endif


static int lwip_sock_make_addr(struct netconn* conn,ip_addr_t* fromaddr,u16 port,struct sockaddr* from,socklen_t* fromlen) {
  int truncated = 0;
  union sockaddr_aligned saddr;

  LWIP_UNUSED_ARG(conn);

  LWIP_ASSERT("fromaddr != NULL", fromaddr != NULL);
  LWIP_ASSERT("from != NULL", from != NULL);
  LWIP_ASSERT("fromlen != NULL", fromlen != NULL);

#if LWIP_IPV4 && LWIP_IPV6
  /* Dual-stack: Map IPv4 addresses to IPv4 mapped IPv6 */
  if (NETCONNTYPE_ISIPV6(netconn_type(conn)) && IP_IS_V4(fromaddr)) {
    ip4_2_ipv4_mapped_ipv6(ip_2_ip6(fromaddr), ip_2_ip4(fromaddr));
    IP_SET_TYPE(fromaddr, IPADDR_TYPE_V6);
  }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

  IPADDR_PORT_TO_SOCKADDR(&saddr, fromaddr, port);
  if (*fromlen < saddr.sa.sa_len) {
    truncated = 1;
  } else if (*fromlen > saddr.sa.sa_len) {
    *fromlen = saddr.sa.sa_len;
  }
  MEMCPY(from, &saddr, *fromlen);
  return truncated;
}


#if LWIP_TCP
/* Helper function to get a tcp socket's remote address info */
static int lwip_recv_tcp_from(struct lwip_sock* sock,struct sockaddr* from,socklen_t* fromlen,const char* dbg_fn,int dbg_s,isize dbg_ret) {
  if (sock == NULL) {
    return 0;
  }
  LWIP_UNUSED_ARG(dbg_fn);
  LWIP_UNUSED_ARG(dbg_s);
  LWIP_UNUSED_ARG(dbg_ret);

#if !SOCKETS_DEBUG
  if (from && fromlen)
#endif /* !SOCKETS_DEBUG */
  {
    /* get remote addr/port from tcp_pcb */
    u16 port;
    ip_addr_t tmpaddr;
    netconn_getaddr(sock->conn, &tmpaddr, &port, 0);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("%s(%d):  addr=", dbg_fn, dbg_s));
    // ip_addr_debug_print_val(SOCKETS_DEBUG, tmpaddr);
    LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F" len=%d\n", port, (int)dbg_ret));
    if (from && fromlen) {
      return lwip_sock_make_addr(sock->conn, &tmpaddr, port, from, fromlen);
    }
  }
  return 0;
}
#endif



static err_t lwip_recvfrom_udp_raw(struct lwip_sock* sock,int flags,struct msghdr* msg,u16* datagram_len,int dbg_s) {
  struct netbuf* buf;
  u8 apiflags;
  err_t err;
  u16 buflen, copylen, copied;
  int i;

  LWIP_UNUSED_ARG(dbg_s);
  LWIP_ERROR("lwip_recvfrom_udp_raw: invalid arguments", (msg->msg_iov != NULL) || (msg->msg_iovlen <= 0), return ERR_ARG;);

  if (flags & MSG_DONTWAIT) {
    apiflags = NETCONN_DONTBLOCK;
  } else {
    apiflags = 0;
  }

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw[UDP/RAW]: top sock->lastdata=%p\n", (void *)sock->lastdata.netbuf));
  /* Check if there is data left from the last recv operation. */
  buf = sock->lastdata.netbuf;
  if (buf == NULL) {
    /* No data was left from the previous operation, so we try to get
        some from the network. */
    err = netconn_recv_udp_raw_netbuf_flags(sock->conn, &buf, apiflags);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw[UDP/RAW]: netconn_recv err=%d, netbuf=%p\n",
                                err, (void *)buf));

    if (err != ERR_OK) {
      return err;
    }
    LWIP_ASSERT("buf != NULL", buf != NULL);
    sock->lastdata.netbuf = buf;
  }
  buflen = buf->p->tot_len;
  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw: buflen=%"U16_F"\n", buflen));

  copied = 0;
  /* copy the pbuf payload into the iovs */
  for (i = 0; (i < msg->msg_iovlen) && (copied < buflen); i++) {
    u16 len_left = (u16)(buflen - copied);
    if (msg->msg_iov[i].iov_len > len_left) {
      copylen = len_left;
    } else {
      copylen = (u16)msg->msg_iov[i].iov_len;
    }

    /* copy the contents of the received buffer into
        the supplied memory buffer */
    pbuf_copy_partial(buf->p, (u8 *)msg->msg_iov[i].iov_base, copylen, copied);
    copied = (u16)(copied + copylen);
  }

  /* Check to see from where the data was.*/
#if !SOCKETS_DEBUG
  if (msg->msg_name && msg->msg_namelen)
#endif /* !SOCKETS_DEBUG */
  {
    // LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw(%d):  addr=", dbg_s));
    // ip_addr_debug_print_val(SOCKETS_DEBUG, *netbuf_fromaddr(buf));
    // LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F" len=%d\n",&buf->addr, copied));
    if (msg->msg_name && msg->msg_namelen) {
      lwip_sock_make_addr(sock->conn,&buf->addr,buf->port,(struct sockaddr*)msg->msg_name, &msg->msg_namelen);
    }
  }

  /* Initialize flag output */
  msg->msg_flags = 0;

  if (msg->msg_control) {
    u8 wrote_msg = 0;
#if LWIP_NETBUF_RECVINFO
    /* Check if packet info was recorded */
    if (buf->flags & NETBUF_FLAG_DESTADDR) {
      if (IP_IS_V4(&buf->toaddr)) {
#if LWIP_IPV4
        if (msg->msg_controllen >= CMSG_SPACE(sizeof(struct in_pktinfo))) {
          struct cmsghdr *chdr = CMSG_FIRSTHDR(msg); /* This will always return a header!! */
          struct in_pktinfo *pkti = (struct in_pktinfo *)CMSG_DATA(chdr);
          chdr->cmsg_level = IPPROTO_IP;
          chdr->cmsg_type = IP_PKTINFO;
          chdr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
          pkti->ipi_ifindex = buf->p->if_idx;
          inet_addr_from_ip4addr(&pkti->ipi_addr, ip_2_ip4(netbuf_destaddr(buf)));
          msg->msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));
          wrote_msg = 1;
        } else {
          msg->msg_flags |= MSG_CTRUNC;
        }
#endif /* LWIP_IPV4 */
      }
    }
#endif /* LWIP_NETBUF_RECVINFO */

    if (!wrote_msg) {
      msg->msg_controllen = 0;
    }
  }

  /* If we don't peek the incoming message: zero lastdata pointer and free the netbuf */
  if ((flags & MSG_PEEK) == 0) {
    sock->lastdata.netbuf = NULL;
    netbuf_delete(buf);
  }
  if (datagram_len) {
    *datagram_len = buflen;
  }
  return ERR_OK;
}


isize lwip_recvfrom(int s,void* mem,usize len,int flags,struct sockaddr* from, socklen_t* fromlen) {
  struct lwip_sock* sock;
  isize ret;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom(%d, %p, %"SZT_F", 0x%x, ..)\n", s, mem, len, flags));
  sock = get_socket(s);
  if (!sock) {
    return -1;
  }
#if LWIP_TCP
  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP) {
    ret = lwip_recv_tcp(sock, mem, len, flags);
    lwip_recv_tcp_from(sock, from, fromlen, "lwip_recvfrom", s, ret);
    done_socket(sock);
    return ret;
  } else
#endif
  {
    u16 datagram_len = 0;
    struct iovec vec;
    struct msghdr msg;
    err_t err;
    vec.iov_base = mem;
    vec.iov_len = len;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    msg.msg_name = from;
    msg.msg_namelen = (fromlen ? *fromlen : 0);
    err = lwip_recvfrom_udp_raw(sock, flags, &msg, &datagram_len, s);
    if (err != ERR_OK) {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom[UDP/RAW](%d): buf == NULL, error is \"%s\"!\n",
                                  s, lwip_strerr(err)));
      sock_set_errno(sock, err_to_errno(err));
      done_socket(sock);
      return -1;
    }
    ret = (isize)LWIP_MIN(LWIP_MIN(len, datagram_len), ISIZE_MAX);
    if (fromlen) {
      *fromlen = msg.msg_namelen;
    }
  }

  sock_set_errno(sock, 0);
  done_socket(sock);
  return ret;
}


struct lwip_socket_multicast_pair {
  /** the socket */
  struct lwip_sock* sock;
  /** the interface address */
  ip4_addr_t if_addr;
  /** the group address */
  ip4_addr_t multi_addr;
};

struct lwip_socket_multicast_pair socket_ipv4_multicast_memberships[LWIP_SOCKET_MAX_MEMBERSHIPS];

static void lwip_socket_drop_registered_memberships(int s) {
  struct lwip_sock* sock=get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if (socket_ipv4_multicast_memberships[i].sock == sock) {
      ip_addr_t multi_addr, if_addr;
      ip_addr_copy_from_ip4(multi_addr, socket_ipv4_multicast_memberships[i].multi_addr);
      ip_addr_copy_from_ip4(if_addr, socket_ipv4_multicast_memberships[i].if_addr);
      socket_ipv4_multicast_memberships[i].sock = NULL;
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].if_addr);
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].multi_addr);

      netconn_join_leave_group(sock->conn, &multi_addr, &if_addr, NETCONN_LEAVE);
    }
  }
  done_socket(sock);
}

int lwip_close(int s) {
  struct lwip_sock* sock;
  int is_tcp=0;
  err_t err;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_close(%d)\n", s));

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (sock->conn != NULL) {
    is_tcp = NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP;
  } else {
    LWIP_ASSERT("sock->lastdata == NULL", sock->lastdata.pbuf == NULL);
  }

#if LWIP_IGMP
  /* drop all possibly joined IGMP memberships */
  lwip_socket_drop_registered_memberships(s);
#endif /* LWIP_IGMP */
#if LWIP_IPV6_MLD
  /* drop all possibly joined MLD6 memberships */
  lwip_socket_drop_registered_mld6_memberships(s);
#endif /* LWIP_IPV6_MLD */

  err = netconn_delete(sock->conn);
  if (err != ERR_OK) {
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return -1;
  }

  free_socket(sock, is_tcp);
  set_errno(0);
  return 0;
}



