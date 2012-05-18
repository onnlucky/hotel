// platform and bit manipulation routines

#pragma once

#include "config.h"

// default includes
#define _GNU_SOURCE
#include <string.h>
#include <math.h>
#include <strings.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/time.h>

#ifdef HAVE_BOEHMGC
#include "libgc/include/gc.h"
#define malloc(n) GC_MALLOC(n)
#define malloc_atomic(n) GC_MALLOC_ATOMIC(n)
#define calloc(m,n) GC_MALLOC((m)*(n))
#define free(p) GC_FREE(p)
#define realloc(p,n) GC_REALLOC((p),(n))
#undef strdup
#define strdup(s) GC_STRDUP((s))
#else
#define GC_INIT()
#define GC_REGISTER_FINALIZER(a1, a2, a3, a4, a5)
#define GC_gcollect()
#define malloc_atomic malloc
#endif

#ifdef HAVE_THREADS
#include <pthread.h>
#endif

#include "llib/lhashmap.h"
#include "llib/lqueue.h"

#include "a_var.h"

#include "debug.h"
#include "trace-off.h"

#ifndef bool
#define bool int
#define true 1
#define false 0
#endif
#ifndef null
#define null ((intptr_t)0)
#endif

#define UNUSED(x) (x = x)
#define PUBLIC

#define min(l, r) (((l) <= (r))?(l):(r))
#define max(l, r) (((l) >= (r))?(l):(r))

#if defined(_AIX) && defined(__64BIT__)
  #define M64
  #define PLATFORM "aix"
  #define PLATFORM_AIX
  #define CPU "ppc64"
#elif defined(_AIX) && !defined(__64BIT__)
  #define M32
  #define PLATFORM "aix"
  #define PLATFORM_AIX
  #define CPU "ppc"
#elif defined(__APPLE__) && defined(__i386__)
  #define M32
  #define PLATFORM "macosx"
  #define PLATFORM_MACOSX
  #define CPU "i386"
#elif defined(__APPLE__) && defined(__x86_64__)
  #define M64
  #define PLATFORM "macosx"
  #define PLATFORM_MACOSX
  #define CPU "x86_64"
#elif defined(__APPLE__) && defined(__ppc64__)
  #define M64
  #define PLATFORM "macosx"
  #define PLATFORM_MACOSX
  #define CPU "ppc64"
#elif defined(__APPLE__) && defined(__ppc__)
  #define M32
  #define PLATFORM "macosx"
  #define PLATFORM_MACOSX
  #define CPU "ppc"
#elif defined(__i386__)
  #define M32
  #define PLATFORM "linux"
  #define PLATFORM_LINUX
  #define CPU "i386"
#elif defined(__x86_64__)
  #define M64
  #define PLATFORM "linux"
  #define PLATFORM_LINUX
  #define CPU "x86_64"
#elif defined(__powerpc__) && !defined(__powerpc64__)
  #define M32
  #define PLATFORM "linux"
  #define PLATFORM_LINUX
  #define CPU "ppc"
#elif defined(__powerpc__) && defined(__powerpc64__)
  #define M64
  #define PLATFORM "linux"
  #define PLATFORM_LINUX
  #define CPU "ppc64"
#elif defined(__WIN32__)
  #define M32
  #define PLATFORM "windows"
  #define PLATFORM_WINDOWS
  #define CPU "i386"
#elif defined(__WIN64__)
  #define M64
  #define PLATFORM "windows"
  #define PLATFORM_WINDOWS
  #define CPU "x86_64"
#else
  #warning "unknown platform/processor, assuming generic 32 bit"
  #define M32
  #define PLATFORM "generic"
  #define PLATFORM_GENERIC
  #define CPU "generic32"
#endif

#if !defined(BYTE_ORDER)
#include <endian.h>
#if !defined(BYTE_ORDER)
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define    BIG_ENDIAN __BIG_ENDIAN
#define    BYTE_ORDER __BYTE_ORDER
#endif
#endif

// n == network ??
#if   BYTE_ORDER == BIG_ENDIAN && BYTE_ORDER != LITTLE_ENDIAN
  #define ui32_from_n(n) n
  #define ui16_from_n(n) n
  #define nendian 1
#elif BYTE_ORDER == LITTLE_ENDIAN && BYTE_ORDER != BIG_ENDIAN
  #define ui32_from_n(n) ((((ui32)(n) & 0xff000000) >> 24) | \
                          (((ui32)(n) & 0x00ff0000) >> 8)  | \
                          (((ui32)(n) & 0x0000ff00) << 8)  | \
                          (((ui32)(n) & 0x000000ff) << 24))
  #define ui16_from_n(n) ((((ui16)(n) & 0xff00) >> 8) | \
                          (((ui16)(n) & 0x00ff) << 8))
  #define nendian 0
#else
  #error "unknown endian type, define BIG_ENDIAN or LITTLE_ENDIAN"
#endif

// don't ask, don't tell
//char* strdup(const char*);
#if defined(__DARWIN_C_LEVEL) && __DARWIN_C_LEVEL <= 200809L
static inline size_t strnlen(const char* s, size_t n) {
    char *p = memchr(s, 0, n);
    return (p?p-s:n);
}
static inline char* strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* ret = malloc(len + 1);
    memcpy(ret, s, len);
    ret[len] = 0;
    return ret;
}
#endif
#ifdef PLATFORM_LINUX
static inline char *strnstr(const char *s1, const char *s2, size_t len) {
   size_t l1 = len, l2;

   l2 = strlen(s2);
   if (!l2) return (char *)s1;
   while (l1 >= l2) {
       l1--;
       if (!memcmp(s1, s2, l2)) return (char *)s1;
       s1++;
   }
   return NULL;
}
#endif

