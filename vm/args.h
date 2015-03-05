#ifndef _args_h_
#define _args_h_

bool tlArgsIsSetter(tlArgs* args);
tlList* tlArgsNames(tlArgs* args);
tlArgs* tlArgsFrom(tlArgs* call, tlHandle fn, tlSym method, tlHandle target);

tlHandle tlInvoke(tlTask* task, tlArgs* call);

#endif
