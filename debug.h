// author: Onne Gorter <onne@onnlucky.com>

#ifndef _debug_h_
#define _debug_h_
#pragma once

#ifndef HAVE_DEBUG
// DISABLE DEBUGGING

#define   print(f, x...) do { fprintf(stderr, f"\n", ##x); fflush(stderr); } while(0)
#define warning(f, x...) do { fprintf(stderr, "warning: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); } while(0)
#define   fatal(f, x...) do { fprintf(stderr, "fatal: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); abort(); } while(0)
#define   debug(f, x...)
#define if_debug(t)

#else
// DEBUGGING

// enable asserts, regardless
#ifndef HAVE_ASSERTS
#define HAVE_ASSERTS
#endif

#define   print(f, x...) do { fprintf(stderr, "PRINT: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); } while(0)
#define warning(f, x...) do { fprintf(stderr, "WARNING: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); } while(0)
#define  fatal(f, x...)  do { fprintf(stderr, "FATAL: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stderr); abort(); } while(0)
#define   debug(f, x...) fprintf(stderr,   "DEBUG: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x)
#define if_debug(t) t

#endif // HAVE_DEBUG

// ASSERTs
#ifdef HAVE_ASSERTS

#ifndef assert
#define assert(t) if (! (t) ) { fprintf(stderr, "%s:%u %s() - assertion failed: '%s'\n", __FILE__, __LINE__, __FUNCTION__, #t); abort(); }
#endif

#else // no HAVE_ASSERTS

#ifndef assert
#define assert(t, x...)
#endif

#endif // HAVE_ASSERTS

#endif // _debug_h_

