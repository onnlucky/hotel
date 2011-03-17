#ifdef HAVE_DEBUG

#undef trace
#define trace(f, x...) do { fprintf(stdout, "trace: %s:%u %s() - "f"\n", __FILE__, __LINE__, __FUNCTION__, ##x); fflush(stdout); } while (0)

#undef if_trace
#define if_trace(f) f

#endif

