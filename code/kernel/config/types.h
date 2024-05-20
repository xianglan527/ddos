#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__

#ifndef NULL
#define NULL 0
#endif

#ifndef nullptr
#define nullptr ((void *)0)
#endif

typedef int bool;

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

#endif /* __CONFIG_TYPES_H__*/