#ifndef _args_h_
#define _args_h_

#include "tl.h"

struct tlArgs {
    tlHead head;
    uint32_t size; // total data size
    uint32_t spec; // 0b...11 count of target/method/names, spec >> 2 = count of names
    tlHandle fn;
    tlHandle data[];
    // first 3 items in data[] are:
    // target; // if spec & 2
    // method; // if spec & 2 (a tlSym)
    // names;  // if spec & 1 (a tlList)
};

tlList* tlArgsList(tlArgs* args);

bool tlArgsIsSetter(tlArgs* args);
tlList* tlArgsNames(tlArgs* args);
tlArgs* tlArgsFrom(tlArgs* call, tlHandle fn, tlSym method, tlHandle target);

void tlArgsMakeSetter_(tlArgs* args);

tlHandle tlInvoke(tlTask* task, tlArgs* call);

void args_init();

#endif
