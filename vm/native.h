#ifndef _native_h_
#define _native_h_

#include "tl.h"

struct tlNative {
    tlHead head;
    tlNativeCb native;
    tlSym name;
};
tlNative* tlNativeNew(tlNativeCb native, tlSym name);
tlSym tlNativeName(tlNative* fn);

void native_init_first();
void native_init();

#endif
