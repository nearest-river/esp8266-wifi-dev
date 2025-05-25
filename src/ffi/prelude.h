#include <stdint.h>
#include <stdlib.h>


#ifndef CPRIMITIVES_H
#define CPRIMITIVES_H


typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

typedef __INT8_TYPE__ i8;
typedef __INT16_TYPE__ i16;
typedef __INT32_TYPE__ i32;
typedef __INT64_TYPE__ i64;

#if __WORDSIZE==64
typedef i64 isize;
typedef u64 usize;
#else
typedef i32 isize;
typedef u32 usize;
#endif

typedef u32 utf8char;

typedef float f32;
typedef double f64;
#ifdef __FLOAT128__
typedef __float128 f128;
#endif



#ifndef __STDBOOL_H
#define __bool_true_false_are_defined 1

#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L
#elif !defined(__cplusplus)
typedef _Bool bool;
#define true 1
#define false 0
#elif defined(__GNUC__) && !defined(__STRICT_ANSI__)
/* Define _Bool as a GNU extension. */
#define _Bool bool
#if defined(__cplusplus) && __cplusplus < 201103L
/* For C++98, define bool, false, true as a GNU extension. */
typedef bool bool;
#define false false
#define true true
#endif
#endif
#endif

#define never  __attribute__((__noreturn__))
#define never_inline __attribute__((__noinline__))

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_FLD_8(x) PACK_STRUCT_FIELD(x)
#define PACK_STRUCT_FLD_S(x) PACK_STRUCT_FIELD(x)


#define USIZE_MAX (~(usize)0U)
#define ISIZE_MAX ((isize)(USIZE_MAX>>1))


#define MIN(X,Y) ((X)<(Y)?(X):(Y))
#define MAX(X,Y) ((X)>(Y)?(X):(Y))

#ifndef BIT
#define BIT(X) (1<<(X))
#endif // !BIT





#define IRAM __attribute__((section(".iram1.text")))
#define LWIP_UNUSED_ARG(x) (void)x
#define PRIVILEGED_FUNCTION __attribute__((section("privileged_functions")))
#define PRIVILEGED_DATA __attribute__((section("privileged_data")))
#define offsetof(TYPE, MEMBER)  __builtin_offsetof (TYPE, MEMBER)




u32 u32_high_to_neutral(u32 x);
u16 u16_high_to_neutral(u16 x);
u32 u32_neutral_to_high(u32 x);
u16 u16_neutral_to_high(u16 x);





#endif

