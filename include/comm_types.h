#ifndef COMM_TYPES_H
#define COMM_TYPES_H

#include <stdint.h>

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#ifndef OS_POSIX
#define OS_POSIX
#endif
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

const int32 RET_OK = 0;
const int32 RET_ERROR = -1;

#endif
