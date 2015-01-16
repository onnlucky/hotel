#include "tests.h"

#include "platform.h"
#include "tl.h"
#include "env.h"

TEST(basic) {
    tlList* names = tlListFrom(tlSYM("hello"), tlSYM("x"), tlSYM("y"), null);
    tlEnv* env = tlEnvNew(names, null);
    REQUIRE(tlEnvSize(env) == 3);

    tlEnvSet_(env, 0, tlSYM("world"));
    tlEnvSet_(env, 1, tlINT(42));
    tlEnvSet_(env, 2, tlINT(100));

    REQUIRE(tlEnvGet(env, 0) == tlSYM("world"));
}

TEST(import) {
    tlList* names1 = tlListFrom(tlSYM("a"), tlSYM("b"), null);
    tlEnv* env1 = tlEnvNew(names1, null);
    tlEnvSet_(env1, 0, tlINT(100));
    tlEnvSet_(env1, 1, tlINT(200));

    tlList* names2 = tlListFrom(tlSYM("x"), tlSYM("y"), null);
    tlEnv* env2 = tlEnvNew(names2, env1);
    tlEnvSet_(env2, 0, tlINT(1));
    tlEnvSet_(env2, 1, tlINT(2));

    tlObject* imports = tlObjectFrom("hello", tlSYM("world"), "b", tlINT(2000), "y", tlINT(9), null);
    tlEnvImport_(env1, imports);
    REQUIRE(tlEnvGetName(env1, tlSYM("hello"), INT_MAX) == tlSYM("world"));
    REQUIRE(tlEnvGetName(env2, tlSYM("hello"), INT_MAX) == tlSYM("world"));

    REQUIRE(tlEnvGetFull(env2, 0, 0) == tlINT(1));
    REQUIRE(tlEnvGetFull(env2, 0, 1) == tlINT(2));
    REQUIRE(tlEnvGetFull(env2, 1, 0) == tlINT(100));
    REQUIRE(tlEnvGetFull(env2, 1, 1) == tlINT(2000));
}

int main(int argc, char** argv) {
    tl_init();
    RUN(basic);
    RUN(import);
}
