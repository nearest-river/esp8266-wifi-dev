#ifndef IP_ADDR_H
#define IP_ADDR_H
#include "prelude.h"
#include "def.h"



#define IP6_NO_ZONE 0
#define IPADDR_ANY ((u32)0x00000000UL)

extern const ip_addr_t ip_addr_any;
#define IP4_ADDR_ANY (&ip_addr_any)


#if LWIP_IPV6_SCOPES
#define ip6_addr_clear_zone(ip6addr) ((ip6addr)->zone = IP6_NO_ZONE)
#else
#define ip6_addr_clear_zone(ip6addr) 
#endif

/** Set complete address to zero */
#define ip6_addr_set_zero(ip6addr) do{(ip6addr)->addr[0] = 0; \
                                         (ip6addr)->addr[1] = 0; \
                                         (ip6addr)->addr[2] = 0; \
                                         (ip6addr)->addr[3] = 0; \
                                         ip6_addr_clear_zone(ip6addr);}while(0)

#define ip6_addr_set_any(ip6addr) ip6_addr_set_zero(ip6addr)

#define ip_2_ip6(ipaddr) (&((ipaddr)->u_addr.ip6))
#define ip_2_ip4(ipaddr) (&((ipaddr)->u_addr.ip4))

#define IP_SET_TYPE_VAL(ipaddr, iptype) do { (ipaddr).type = (iptype); }while(0)
#define IP_SET_TYPE(ipaddr, iptype)     do { if((ipaddr) != NULL) { IP_SET_TYPE_VAL(*(ipaddr), iptype); }}while(0)

#define ip_clear_no4(ipaddr) do { ip_2_ip6(ipaddr)->addr[1] = \
                                   ip_2_ip6(ipaddr)->addr[2] = \
                                   ip_2_ip6(ipaddr)->addr[3] = 0; \
                                   ip6_addr_clear_zone(ip_2_ip6(ipaddr)); }while(0)

#define ip4_addr_set_any(ipaddr) ((ipaddr)->addr = IPADDR_ANY)

#define ip_addr_set_any(is_ipv6, ipaddr) do{if(is_ipv6){ \
  ip6_addr_set_any(ip_2_ip6(ipaddr)); IP_SET_TYPE(ipaddr, IPADDR_TYPE_V6); }else{ \
  ip4_addr_set_any(ip_2_ip4(ipaddr)); IP_SET_TYPE(ipaddr, IPADDR_TYPE_V4); ip_clear_no4(ipaddr); }}while(0)

#define IP_GET_TYPE(ipaddr)           ((ipaddr)->type)
#define IP_IS_V6_VAL(ipaddr)          (IP_GET_TYPE(&ipaddr) == IPADDR_TYPE_V6)

#define ip6_addr_isipv4mappedipv6(ip6addr) (((ip6addr)->addr[0] == 0) && ((ip6addr)->addr[1] == 0) && (((ip6addr)->addr[2]) == PP_HTONL(0x0000FFFFUL)))
#define unmap_ipv4_mapped_ipv6(ip4addr, ip6addr) \
  (ip4addr)->addr = (ip6addr)->addr[3];


#define ip4_2_ipv4_mapped_ipv6(ip6addr, ip4addr) do { \
  (ip6addr)->addr[3] = (ip4addr)->addr; \
  (ip6addr)->addr[2] = PP_HTONL(0x0000FFFFUL); \
  (ip6addr)->addr[1] = 0; \
  (ip6addr)->addr[0] = 0; \
  ip6_addr_clear_zone(ip6addr); } while(0);

#define IP_IS_ANY_TYPE_VAL(ipaddr)    (IP_GET_TYPE(&ipaddr) == IPADDR_TYPE_ANY)

#define ip4_addr_copy(dest, src) ((dest).addr = (src).addr)
#define ip_addr_copy_from_ip4(dest, src)      do{ \
  ip4_addr_copy(*ip_2_ip4(&(dest)), src); IP_SET_TYPE_VAL(dest, IPADDR_TYPE_V4); ip_clear_no4(&dest); }while(0)
#define ip4_addr_set_zero(ipaddr)     ((ipaddr)->addr = 0)



#if LWIP_IPV6 & LWIP_IPV4
#define IPADDR4_INIT(u32val)          { { { { u32val, 0ul, 0ul, 0ul } IPADDR6_ZONE_INIT } }, IPADDR_TYPE_V4 }
#endif

#if LWIP_IPV4
#define IPADDR4_INIT(u32val)                    { u32val }
#endif






#endif // !IP_ADDR_H
