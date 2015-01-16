#include "tests.h"

#include "platform.h"
#include "tl.h"

TEST(basic) {
    tlArgs* args = tlArgsNew(3);
    REQUIRE(tlArgsSize(args) == 3);
    REQUIRE(tlArgsNamedSize(args) == 0);

    tlArgsSet_(args, 0, tlINT(42));
    tlArgsSet_(args, 1, tlINT(43));
    tlArgsSet_(args, 2, tlINT(44));

    REQUIRE(tlArgsGet(args, 0) == tlINT(42));
    REQUIRE(tlArgsGet(args, 1) == tlINT(43));
    REQUIRE(tlArgsGet(args, 2) == tlINT(44));
}

TEST(named) {
    tlArgs* args = tlArgsNewNames(5, true, false);
    tlArgsSet_(args, 0, tlINT(42));
    tlArgsSet_(args, 1, tlINT(43));
    tlArgsSet_(args, 2, tlINT(44));
    tlArgsSet_(args, 3, tlINT(45));
    tlArgsSet_(args, 4, tlINT(46));

    REQUIRE(tlArgsSize(args) == 5);

    tlList* names = tlListFrom(tlNull, tlSYM("hello"), tlNull, tlSYM("world"), tlNull, null);
    tlArgsSetNames_(args, names, 0);

    REQUIRE(tlArgsSize(args) == 3);
    REQUIRE(tlArgsNamedSize(args) == 2);

    REQUIRE(tlArgsGet(args, 2) == tlINT(46)); // third non named argument
    REQUIRE(tlArgsGetNamed(args, tlSYM("hello")) == tlINT(43));
    REQUIRE(tlArgsGetNamed(args, tlSYM("world")) == tlINT(45));
    REQUIRE(tlArgsGetNamed(args, tlSYM("block")) == null);
    REQUIRE(tlArgsBlock(args) == 0);
}

TEST(method) {
    tlArgs* args = tlArgsNewNames(5, false, true);
    tlArgsSet_(args, 0, tlINT(42));
    tlArgsSet_(args, 1, tlINT(43));
    tlArgsSet_(args, 2, tlINT(44));
    tlArgsSet_(args, 3, tlINT(45));
    tlArgsSet_(args, 4, tlINT(46));

    REQUIRE(tlArgsGet(args, 0) == tlINT(42));
    REQUIRE(tlArgsGet(args, 4) == tlINT(46));
    REQUIRE(tlArgsTarget(args) == null);
    REQUIRE(tlArgsMethod(args) == null);

    tlArgsSetTarget_(args, tlINT(1000));
    tlArgsSetMethod_(args, tlSYM("test"));
    REQUIRE(tlArgsTarget(args) == tlINT(1000));
    REQUIRE(tlArgsMethod(args) == tlSYM("test"));

    REQUIRE(tlArgsFn(args) == null);
    tlArgsSetFn_(args, tlINT(2000));

    REQUIRE(tlArgsFn(args) == tlINT(2000));
    REQUIRE(tlArgsTarget(args) == tlINT(1000));
    REQUIRE(tlArgsMethod(args) == tlSYM("test"));

    REQUIRE(tlArgsGet(args, 0) == tlINT(42));
    REQUIRE(tlArgsGet(args, 4) == tlINT(46));
}

TEST(raw) {
    tlArgs* args = tlArgsNewNames(5, true, false);
    tlList* names = tlListFrom(tlNull, tlSYM("hello"), tlNull, tlSYM("world"), tlNull, null);
    tlArgsSetNames_(args, names, 2);

    tlArgsSet_(args, 0, tlINT(42));
    tlArgsSet_(args, 1, tlINT(43));
    tlArgsSet_(args, 2, tlINT(44));
    tlArgsSet_(args, 3, tlINT(45));
    tlArgsSet_(args, 4, tlINT(46));

    REQUIRE(tlArgsRawSize(args) == 5);
    REQUIRE(tlArgsSize(args) == 3);
    REQUIRE(tlArgsNamedSize(args) == 2);
}

int main(int argc, char** argv) {
    tl_init();

    RUN(basic);
    RUN(named);
    RUN(method);
    RUN(raw);
}

