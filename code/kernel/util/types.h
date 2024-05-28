#ifndef __UTIL_TYPES_H__
#define __UTIL_TYPES_H__

#ifndef NULL
#define NULL 0
#endif

#ifndef nullptr
#define nullptr ((void *)0)
#endif

typedef unsigned int bool;

#define bool _Bool
#define true 1
#define false 0

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef long long intptr_t;
typedef uint64_t uintptr_t;

typedef uintptr_t size_t;

typedef size_t ppn_t;

#define ROUNDDOWN(a, n)                \
    ({                                 \
        size_t __a = (size_t)(a);      \
        (typedef(a))(__a - __a % (n)); \
    })

#define ROUNDUP(a, n)                                        \
    ({                                                       \
        size_t __n = (size_t)(n);                            \
        (typedef(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n)); \
    })

#define offsetof(type, member) \
        ((size_t)(&(type *)0)->member))

#define to_struct(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* __CONFIG_TYPES_H__*/