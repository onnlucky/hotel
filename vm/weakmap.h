#ifndef _weakmap_h_
#define _weakmap_h_

#include "tl.h"

TL_REF_TYPE(tlWeakMap);
tlWeakMap* tlWeakMapNew();

int tlWeakMapSize(tlWeakMap* map);
tlHandle tlWeakMapGet(tlWeakMap* map, tlHandle key);
tlHandle tlWeakMapSet(tlWeakMap* map, tlHandle key, tlHandle value);
tlHandle tlWeakMapDel(tlWeakMap* map, tlHandle key);
void weakmap_init();

#endif
