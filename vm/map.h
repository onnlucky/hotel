#ifndef _map_h_
#define _map_h_

#include "tl.h"

struct tlMap {
    tlHead head;
    tlSet* keys;
    tlHandle data[];
};

void tlMapSet_(tlMap* map, tlHandle key, tlHandle v);
tlMap* tlMapFromObject_(tlObject* o);
tlObject* tlObjectFromMap_(tlMap* map);

void map_init();

#endif
