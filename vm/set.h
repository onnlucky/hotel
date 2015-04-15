#ifndef _set_h_
#define _set_h_

#include "tl.h"

struct tlSet {
    tlHead head;
    intptr_t size;
    tlHandle data[];
};

tlSet* tlSetCopy(tlSet* set, int size);
tlSet* tlSetDel(tlSet* set, tlSym key, int* at);

int tlSetSize(tlSet* set);
int tlSetIndexof(tlSet* set, tlHandle key);

void set_init_first();
void set_init();

#endif
