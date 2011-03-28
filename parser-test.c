#include "tl.h"
#include "debug.h"

#include "platform.h"
#include "value.c"

#include "list.c"
#include "map.c"

#include "text.c"
#include "sym.c"


int main() {
    sym_init();
    list_init();
    map_init();

    tValue v = parse(tTEXT(null, "one=1, two=2, three=3, 1, 2, 3, +a, -b, -one, -one, -one, -one"));
    if (tmap_is(v)) tmap_dump(tmap_as(v));
    print("---DONE---");
}

