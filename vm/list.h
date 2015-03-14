#ifndef _list_h_
#define _list_h_

#include "tl.h"

struct tlList {
    tlHead head;
    intptr_t size;
    tlHandle data[];
};

tlList* tlListFrom1(tlHandle v1);
tlList* tlListFrom2(tlHandle v1, tlHandle v2);
tlList* tlListFrom3(tlHandle v1, tlHandle v2, tlHandle v3);
tlList* tlListFrom(tlHandle v1, ...);

void list_init();

#endif
