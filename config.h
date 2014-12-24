// various config
#define TL_VERSION "0.2.0"

// if set use pthread library and run multi threaded (otherwise it doesn't work)
#define HAVE_THREADS

// if set use the boehmgc (otherwise we just leak)
#define HAVE_BOEHMGC

// if set use debug and such
#define HAVE_DEBUG

// if set use internal asserts
#define HAVE_ASSERT

#define INTERNAL static
