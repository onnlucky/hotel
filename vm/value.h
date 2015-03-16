#ifndef _value_h_
#define _value_h_

#include "tl.h"

int sub_offset(int first, int last, int size, int* offset);
int at_offset_raw(tlHandle v);
int at_offset(tlHandle v, int size);
int at_offset_min(tlHandle v, int size);
int at_offset_max(tlHandle v, int size);

double tlIntToDouble(tlHandle v);
intptr_t tlIntToInt(tlHandle v);

extern tlSym s__methods;
extern tlSym s__get;
extern tlSym s__set;

extern tlSym s_this;
extern tlSym s_block;
extern tlSym s_class;
extern tlSym s_call;
extern tlSym s_var;
extern tlSym s_method;
extern tlSym s_methods;

extern tlSym s_break;
extern tlSym s_continue;

void value_init();

#endif
