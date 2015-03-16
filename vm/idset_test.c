// author: Onne Gorter, license: MIT (see license.txt)

#include "platform.h"
#include "idset.h"

#include "tests.h"

TEST(basic) {
    tlIdSet* map = tlIdSetNew();
    tlIdSetAdd(map, tlSYM("hello"));
    REQUIRE(tlIdSetSize(map) == 1);
    REQUIRE(tlIdSetHas(map, tlSYM("hello")));
    REQUIRE(!tlIdSetHas(map, tlSYM("foo")));
    REQUIRE(!tlIdSetHas(map, tlSYM("bar")));

    int ref = tlIdSetDel(map, tlSYM("hello"));
    REQUIRE(ref >= 0);
    REQUIRE(tlIdSetSize(map) == 0);
    REQUIRE(!tlIdSetHas(map, tlSYM("hello")));
    REQUIRE(!tlIdSetHas(map, tlSYM("foo")));
    REQUIRE(!tlIdSetHas(map, tlSYM("bar")));

    tlIdSetAdd(map, tlSYM("hello"));
    REQUIRE(tlIdSetSize(map) == 1);
    tlIdSetAdd(map, tlSYM("hello"));
    REQUIRE(tlIdSetSize(map) == 1);

    for (int i = 0; i < tlIdSetSize(map); i++) {
        REQUIRE(tlIdSetGet(map, i) == tlSYM("hello"));
    }

    tlIdSetAdd(map, tlSYM("foobar"));

    for (int i = 0; i < tlIdSetSize(map); i++) {
        REQUIRE(tlIdSetGet(map, i) == tlSYM("hello") || tlIdSetGet(map, i) == tlSYM("foobar"));
    }
}

TEST(grow_shrink) {
    tlIdSet* map = tlIdSetNew();

    const int TESTSIZE = 10 * 1000;
    char buf[1024];
    int len;
    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlSym value = tlSymFromCopy(buf, len);
        tlIdSetAdd(map, value);
    }
    REQUIRE(tlIdSetSize(map) == TESTSIZE);

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlSym value = tlSymFromCopy(buf, len);
        REQUIRE(tlIdSetHas(map, value));
    }

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlSym value = tlSymFromCopy(buf, len);
        REQUIRE(tlIdSetDel(map, value) >= 0);
    }

    for (int i = 0; i < TESTSIZE / 7 * 6; i++) {
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlSym value = tlSymFromCopy(buf, len);
        tlIdSetAdd(map, value);
    }

    for (int i = 0; i < TESTSIZE; i++) {
        len = snprintf(buf, sizeof(buf), "value-%d", i);
        tlSym value = tlSymFromCopy(buf, len);
        REQUIRE(tlIdSetHas(map, value));
    }
    REQUIRE(tlIdSetSize(map) == TESTSIZE);
}

int main(int argc, char** argv) {
    tl_init();
    RUN(basic);
    RUN(grow_shrink);
}

