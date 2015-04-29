// various config
#ifndef TL_PREFIX_RAW
#define TL_PREFIX_RAW /usr/local
#endif
#ifndef TL_VERSION_RAW
#define TL_VERSION_RAW 0.3.2
#endif

#define TL_PREFIX_STR_(x) #x
#define TL_PREFIX_STR(x) TL_PREFIX_STR_(x)
#define TL_PREFIX TL_PREFIX_STR(TL_PREFIX_RAW)
#define TL_VERSION_STR_(x) #x
#define TL_VERSION_STR(x) TL_VERSION_STR_(x)
#define TL_VERSION TL_VERSION_STR(TL_VERSION_RAW)

// if set use pthread library and run multi threaded (otherwise it doesn't work)
#define HAVE_THREADS

// if set use the boehmgc (otherwise we just leak)
#define HAVE_BOEHMGC

// if set use debug and such
#define HAVE_DEBUG

// if set use internal asserts
#define HAVE_ASSERT
