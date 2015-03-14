#ifndef _sym_h_
#define _sym_h_

#include "tl.h"
#include "../llib/lhashmap.h"

int tlSymSize(tlSym sym);

tlHandle tl_global(tlSym sym);

extern LHashMap *globals;

void sym_init_first();
void sym_string_init();

#endif
