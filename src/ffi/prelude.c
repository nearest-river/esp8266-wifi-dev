#include "prelude.h"

#define cfg_builtin(ident) defined(__has_builtin) && __has_builtin(ident)


inline
u32 u32_high_to_neutral(u32 x) {
  #if cfg_builtin(__builtin_bswap32)
    return __builtin_bswap32(x);
  #else
    return ((x & 0x000000ff) << 24) | ((x & 0x0000ff00) << 8) | ((x & 0x00ff0000) >> 8) | ((x & 0xff000000) >> 24);
  #endif
}

inline
u16 u16_high_to_neutral(u16 x) {
  #if cfg_builtin(__builtin_bswap16)
    return __builtin_bswap16(x);
  #else
    return (x << 8) | (x >> 8);
  #endif
}

inline
u32 u32_neutral_to_high(u32 x) {
  #if cfg_builtin(__builtin_bswap32)
    return __builtin_bswap32(x);
  #else
    return (x >> 24)| ((x >> 8) & 0x0000ff00) | ((x << 8) & 0x00ff0000) | (x << 24);
  #endif
}

inline
u16 u16_neutral_to_high(u16 x) {
  #if cfg_builtin(__builtin_bswap16)
    return __builtin_bswap16(x);
  #else
    return (x << 8) | (x >> 8);
  #endif

}





#undef cfg_builtin

