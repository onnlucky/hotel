#ifndef _eval_h_
#define _eval_h_

#include "tl.h"

extern tlString* _t_unknown;
extern tlString* _t_anon;
extern tlString* _t_native;

tlResult* tlResultFromArgs(tlArgs* args);
tlResult* tlResultFromArgsSkipOne(tlArgs* args);

tlHandle _call(tlTask* task, tlArgs* args);

void eval_init();

#endif
