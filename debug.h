// author: Onne Gorter <onne@onnlucky.com>

#ifndef _debug_h_
#define _debug_h_
#pragma once

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define print(f, x...) do { fprintf(stdout, f"\n", ##x); fflush(stdout); } while(0)
#define eprint(f, x...) do { fprintf(stderr, f"\n", ##x); fflush(stderr); } while(0)
#define TODO() do { fprintf(stderr, "FATAL: %s:%u %s() - TODO\n", __FILE__, __LINE__, __FUNCTION__); abort(); } while(0)
#define fatal(f, x...) do { fprintf(stdout, "FATAL: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stdout); abort(); } while(0)

#define STRACE 0

/* DEBUGing macros */
#ifdef HAVE_DEBUG

#undef print
#undef eprint
#undef fatal

#define  print(f, x...) do { fprintf(stdout, "%s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stdout); } while(0)
#define eprint(f, x...) do { fprintf(stderr, "%s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stdout); } while(0)
#define fatal(f, x...)  do { fprintf(stderr, "FATAL: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); abort(); } while(0)

//#ifndef HAVE_STRACE
//#define HAVE_STRACE
//#endif

#ifndef HAVE_ASSERTS
#define HAVE_ASSERTS
#endif

//#ifndef HAVE_API_STRACE
//#define HAVE_API_STRACE
//#endif

#ifndef HAVE_API_ASSERTS
#define HAVE_API_ASSERTS
#endif

#define warning(f, x...) fprintf(stderr, "WARNING: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define   debug(f, x...) fprintf(stderr,   "debug: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define  ddebug(f, x...) fprintf(stderr,   "debug: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define dddebug(f, x...) fprintf(stderr,   "debug: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define if_debug(t) t

#else // no HAVE_DEBUG

#define warning(f, x...) fprintf(stderr, "warning: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define   debug(f, x...)
#define  ddebug(f, x...)
#define dddebug(f, x...)
#define if_debug(t)

#endif // HAVE_DEBUG

/* STRACing macros */
#ifdef HAVE_STRACE
#ifndef HAVE_API_ASSERTS
#define HAVE_API_ASSERTS
#endif

#define strace(f, x...)        if (STRACE) fprintf(stderr, "strace: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)

#else // no HAVE_STRACE

#define strace(f, x...)

#endif // HAVE_STRACE

/* HAVE_API_ASSERTS STRACing macros */
#ifdef HAVE_API_STRACE
#ifndef HAVE_API_ASSERTS
#define HAVE_API_ASSERTS
#endif

#define api_strace(f, x...) fprintf(stderr, "call: %s:%u %s() : "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)

#else // no HAVE_API_STRACE

#define api_strace(f, x...)

#endif

/* HAVE_ASSERTS macros */
#ifdef HAVE_ASSERTS

#ifndef HAVE_API_ASSERTS
#define HAVE_API_ASSERTS
#endif

#ifndef assert
#define assert(t) if (! (t) ) { fprintf(stderr, "%s:%u %s() - assertion failed: "#t"\n", __FILE__, __LINE__, __FUNCTION__); abort(); }
#endif

#else // no HAVE_ASSERTS

#ifndef assert
#define assert(t, x...)
#endif

#endif // HAVE_ASSERTS

/* HAVE_API_ASSERTS macros */
#ifdef HAVE_API_ASSERTS

#define api_assert(t, f, x...) if (! (t) ) { fprintf(stderr, "%s:%u %s() - assertion failed: "#t": ", __FILE__, __LINE__, __FUNCTION__); fprintf(stderr, f"\n", ##x); abort(); }

#else // no HAVE_API_ASSERTS

#define api_assert(t, f, x...)

#endif // HAVE_API_ASSERTS

#endif // _debug_h_

