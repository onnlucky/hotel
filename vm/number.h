#ifndef _number_h_
#define _number_h_

#include "tl.h"

tlNum* tlNumTo(tlHandle h);
tlNum* tlNumNeg(tlNum* num);
tlNum* tlNumAdd(tlNum* l, tlNum* r);
tlNum* tlNumSub(tlNum* l, tlNum* r);
tlNum* tlNumMul(tlNum* l, tlNum* r);
tlNum* tlNumDiv(tlNum* l, tlNum* r);
tlNum* tlNumMod(tlNum* l, tlNum* r);
tlNum* tlNumPow(tlNum* l, intptr_t r);

void number_init();

#endif
