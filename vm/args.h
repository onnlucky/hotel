#ifndef _args_h_
#define _args_h_

tlList* tlArgsNames(tlArgs* args);
tlArgs* tlArgsFrom(tlArgs* call, tlHandle fn, tlSym method, tlHandle target);

tlHandle tlInvoke(tlTask* task, tlArgs* call);

#endif
