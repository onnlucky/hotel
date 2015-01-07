#ifndef _idset_h_
#define _idset_h_

#include "tl.h"

TL_REF_TYPE(tlIdSet);
tlIdSet* tlIdSetNew();
int tlIdSetSize(tlIdSet* set);
bool tlIdSetHas(tlIdSet* set, tlHandle value);
tlHandle tlIdSetGet(tlIdSet* set, int at);
bool tlIdSetAdd(tlIdSet* set, tlHandle value);
bool tlIdSetDel(tlIdSet* set, tlHandle value);

void idset_init();

#endif
