#include "ip_addr4.h"
#include "prelude.h"


inline
static bool is_digit(char c) {
  return c>='0' && c<='9';
}

inline
static bool is_xdigit(char c) {
  return (c>='0' && c<='9') || (c>='A' && c<='F') || (c>='a' && c<='f');
}

inline
static bool is_lower(char c) {
  return c>='a' && c<='z';
}

inline
static bool is_whitespace(char c) {
  return c==' ' || c=='\f' || c=='\n' || c=='\r' || c=='\t' || c=='\v';
}



u32 ipaddr_addr(const char* cp) {
  ip4_addr_t val;

  if (ip4addr_aton(cp, &val)) {
    return ip4_addr_get_u32(&val);
  }
  return (IPADDR_NONE);
}


int ip4addr_aton(const char* cp, ip4_addr_t* addr) {
  u32 val;
  u8 base;
  char c;
  u32 parts[4];
  u32* pp = parts;

  c = *cp;
  for (;;) {
    /*
     * Collect number up to ``.''.
     * Values are specified as for C:
     * 0x=hex, 0=octal, 1-9=decimal.
     */
    if(!is_digit(c)) {
      return 0;
    }
    val = 0;
    base = 10;
    if (c == '0') {
      c = *++cp;
      if (c == 'x' || c == 'X') {
        base = 16;
        c = *++cp;
      } else {
        base = 8;
      }
    }
    for (;;) {
      if(is_digit(c)) {
        val = (val * base) + (u32)(c - '0');
        c = *++cp;
      } else if(base == 16 && is_xdigit(c)) {
        val = (val << 4) | (u32)(c + 10 - (is_lower(c) ? 'a' : 'A'));
        c = *++cp;
      } else {
        break;
      }
    }
    if (c == '.') {
      /*
       * Internet format:
       *  a.b.c.d
       *  a.b.c   (with c treated as 16 bits)
       *  a.b (with b treated as 24 bits)
       */
      if (pp >= parts + 3) {
        return 0;
      }
      *pp++ = val;
      c = *++cp;
    } else {
      break;
    }
  }
  /*
   * Check for trailing characters.
   */
  if (c != '\0' && !is_whitespace(c)) {
    return 0;
  }
  /*
   * Concoct the address according to
   * the number of parts specified.
   */
  switch (pp - parts + 1) {

    case 0:
      return 0;       /* initial nondigit */

    case 1:             /* a -- 32 bits */
      break;

    case 2:             /* a.b -- 8.24 bits */
      if (val > 0xffffffUL) {
        return 0;
      }
      if (parts[0] > 0xff) {
        return 0;
      }
      val |= parts[0] << 24;
      break;

    case 3:             /* a.b.c -- 8.8.16 bits */
      if (val > 0xffff) {
        return 0;
      }
      if ((parts[0] > 0xff) || (parts[1] > 0xff)) {
        return 0;
      }
      val |= (parts[0] << 24) | (parts[1] << 16);
      break;

    case 4:             /* a.b.c.d -- 8.8.8.8 bits */
      if (val > 0xff) {
        return 0;
      }
      if ((parts[0] > 0xff) || (parts[1] > 0xff) || (parts[2] > 0xff)) {
        return 0;
      }
      val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
      break;
    default:
      // LWIP_ASSERT("unhandled", 0);
      break;
  }

  if (addr) {
    ip4_addr_set_u32(addr, u32_high_to_neutral(val));
  }
  return 1;
}



