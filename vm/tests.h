#include <stdio.h>

#define TEST(test) static void test_##test(const char* name, int* count)
#define RUN(test) do { \
        int count = 0; \
        test_##test(#test, &count); \
        if (count == 0) printf("test '" #test "' ran\n"); \
        if (count > 0) printf("test '" #test "' passed; %d requires\n", count); \
        fflush(stdout); \
    } while(0)
#define REQUIRE(c) do{ (*count)++; if (!(c)) { printf("!! test '%s' FAILED: %s:%d " #c "\n", name, __FILE__, __LINE__); *count = -(*count); return; }}while(0)

