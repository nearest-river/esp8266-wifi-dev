#ifndef LWIP_TYPES_H
#define LWIP_TYPES_H
#include "../prelude.h"


typedef int sys_sem_t;
typedef unsigned int nfds_t;
typedef u8 sys_mbox_t;
typedef int sys_prot_t;
typedef u8 sys_mutex_t;
typedef u32 sys_thread_t;

typedef u32 in_addr_t;
struct in_addr {
  in_addr_t s_addr;
};



enum lwip_ip_addr_type {
  /** IPv4 */
  IPADDR_TYPE_V4=0U,
  /** IPv6 */
  IPADDR_TYPE_V6=6U,
  /** IPv4+IPv6 ("dual-stack") */
  IPADDR_TYPE_ANY=46U
};

/** ip4_addr_t uses a struct for convenience only, so that the same defines can
 * operate both on ip4_addr_t as well as on ip4_addr_p_t. */
typedef struct ip4_addr {
  u32 addr;
} ip4_addr_t;


/** This is the aligned version of ip6_addr_t,
    used as local variable, on the stack, etc. */
typedef struct ip6_addr {
  u32 addr[4];
#if LWIP_IPV6_SCOPES
  u8 zone;
#endif /* LWIP_IPV6_SCOPES */
} ip6_addr_t;

/**
 * @ingroup ipaddr
 * A union struct for both IP version's addresses.
 * ATTENTION: watch out for its size when adding IPv6 address scope!
 */
typedef struct ip_addr {
  union {
    ip6_addr_t ip6;
    ip4_addr_t ip4;
  } u_addr;
  /** @ref lwip_ip_addr_type */
  u8 type;
} ip_addr_t;



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
