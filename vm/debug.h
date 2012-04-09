// author: Onne Gorter <onne@onnlucky.com>

void tlabort();

#ifndef _debug_h_
#define _debug_h_
#ifndef HAVE_DEBUG
// DISABLE DEBUGGING

#define   print(f, x...) do { fprintf(stderr, f"\n", ##x); fflush(stderr); } while(0)
#define warning(f, x...) do { fprintf(stderr, "warning: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); } while(0)
#define   fatal(f, x...) do { fprintf(stderr, "fatal: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); tlabort(); } while(0)
#define   debug(f, x...)
#define if_debug(t)

#else
// DEBUGGING

// enable asserts, regardless
#ifndef HAVE_ASSERT
#define HAVE_ASSERT
#endif

#define   print(f, x...) do { fprintf(stderr, "PRINT: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); } while(0)
#define warning(f, x...) do { fprintf(stderr, "WARNING: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); } while(0)
#define  fatal(f, x...)  do { fprintf(stderr, "FATAL: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); tlabort(); } while(0)
#define   debug(f, x...) fprintf(stderr,   "DEBUG: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define if_debug(t) t

#endif // HAVE_DEBUG
#endif // _debug_h_

// ASSERTs
#ifdef HAVE_ASSERT

#ifdef assert
#undef assert
#endif
#define assert(t) if (! (t) ) { fprintf(stderr, "%s:%u %s() - assertion failed: '%s'\n", __FILE__, __LINE__, __FUNCTION__, #t); tlabort(); }

#else // no HAVE_ASSERT

#ifdef assert
#undef assert
#endif
#define assert(t, x...)

#endif // HAVE_ASSERT


