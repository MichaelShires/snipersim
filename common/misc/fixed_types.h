#ifndef __FIXED_TYPES_H
#define __FIXED_TYPES_H

#if defined(PIN_CRT)
#include "pin.H"
// PinCRT provides its own basic types and macros
#else
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <cstdio>
#include <cstring>
#endif

// We define __STDC_LIMIT_MACROS and then include stdint.h
// But if someone else already included stdint.h without first defining __STDC_LIMIT_MACROS,
// UINT64_MAX and friends will not be defined. Test for this here.
#ifndef UINT64_MAX
# if defined(PIN_CRT)
#  define UINT64_MAX 0xffffffffffffffffULL
# else
#  error "UINT64_MAX is not defined. Make sure fixed_types.h is first in the include order."
# endif
#endif

#ifndef PRId64
# if defined(PIN_CRT)
#  define PRId64 "ld"
#  define PRIu64 "lu"
#  define PRIx64 "lx"
#  define PRId32 "d"
#  define PRIu32 "u"
#  define PRIx32 "x"
#  define PRIdPTR "ld"
#  define PRIxPTR "lx"
# else
#  error "PRId64 is not defined. Make sure fixed_types.h is first in the include order."
# endif
#endif

#if defined(PIN_CRT)
typedef uint64_t UInt64;
typedef uint32_t UInt32;
typedef uint16_t UInt16;
typedef uint8_t UInt8;

typedef int64_t SInt64;
typedef int32_t SInt32;
typedef int16_t SInt16;
typedef int8_t SInt8;
#else
typedef uint64_t UInt64;
typedef uint32_t UInt32;
typedef uint16_t UInt16;
typedef uint8_t UInt8;

typedef int64_t SInt64;
typedef int32_t SInt32;
typedef int16_t SInt16;
typedef int8_t SInt8;
#endif

typedef UInt8 Byte;
typedef UInt8 Boolean;

#if defined(PIN_CRT)
typedef ADDRINT IntPtr;
#else
typedef uintptr_t IntPtr;
#endif

typedef uintptr_t carbon_reg_t;

// Carbon core types
typedef SInt32 thread_id_t;
typedef SInt32 app_id_t;
typedef SInt32 core_id_t;
typedef SInt32 carbon_thread_t;

#define INVALID_THREAD_ID ((thread_id_t) - 1)
#define INVALID_APP_ID ((app_id_t) - 1)
#define INVALID_CORE_ID ((core_id_t) - 1)
#define INVALID_ADDRESS ((IntPtr) - 1)

#if defined(__cplusplus)
#include <string>
typedef std::string String;
#endif

#if defined(PIN_CRT)
#ifndef strnlen_s
#define strnlen_s(s, n) strnlen(s, n)
#endif
#ifndef sprintf_s
#define sprintf_s(s, n, ...) sprintf(s, __VA_ARGS__)
#endif
#endif

#endif
