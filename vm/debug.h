// author: Onne Gorter <onne@onnlucky.com>

void tlabort();

#ifndef _debug_h_
#define _debug_h_
#ifndef HAVE_DEBUG
// DISABLE DEBUGGING

#define   print(f, x...) ((void)(fprintf(stderr, f"\n", ##x), fflush(stderr), 0))
#define warning(f, x...) ((void)(fprintf(stderr, "warning: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x), fflush(stderr), 0))
#define   fatal(f, x...) ((void)(fprintf(stderr, "fatal: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x), fflush(stderr), tlabort(), 0))
#define   debug(f, x...)
#define if_debug(t)

#else
// DEBUGGING

// enable asserts, regardless
#ifndef HAVE_ASSERT
#define HAVE_ASSERT
#endif

#define   print(f, x...) ((void)(fprintf(stderr, "PRINT: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x), fflush(stderr), 0))
#define warning(f, x...) ((void)(fprintf(stderr, "WARNING: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x), fflush(stderr), 0))
#define  fatal(f, x...)  ((void)(fprintf(stderr, "FATAL: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x), fflush(stderr), tlabort(), 0))
#define   debug(f, x...) fprintf(stderr,   "DEBUG: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define if_debug(t) t

#endif // HAVE_DEBUG
#endif // _debug_h_

// ASSERTs
#ifdef HAVE_ASSERT

#ifdef assert
#undef assert
#endif
#define assert(e) ((void)((e)? 0: fatal("assertion failed: '%s'", #e)))

#else // no HAVE_ASSERT

#ifdef assert
#undef assert
#endif
#define assert(e) ((void)0)

#endif // HAVE_ASSERT


