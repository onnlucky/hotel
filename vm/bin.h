#ifndef _bin_h_
#define _bin_h_

#include "tl.h"

struct tlBin {
    tlHead head;
    bool interned; // same as string; TODO remove, but for binEquals
    unsigned int hash;
    unsigned int len;
    const char* data;
};

tlBin* tlBinFrom2(tlHandle b1, tlHandle b2);
tlBin* tlBinCat(tlBin* b1, tlBin* b2);

void bin_init();

#endif
