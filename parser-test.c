#include "tl.h"
#include "debug.h"
#include "vm.c"

int main() {
    tvm_init();
    tValue v = parse(tTEXT("one=1, two=2, three=3, 1, 2, 3, +a, -b, -one, -one, -one, -one"));
    if (tmap_is(v)) tmap_dump(tmap_as(v));
    print("---DONE---");
}

