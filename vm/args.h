#ifndef _args_h_
#define _args_h_

#include "tl.h"

enum { kIsSetter = 1 };

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

static inline int tlArgsSizeInline(tlArgs* args) {
    return args->size - (args->spec >> 2) - (args->spec & 3);
}

static inline int tlArgsNamedSizeInline(tlArgs* args) {
    return args->spec >> 2;
}

static inline int tlArgsRawSizeInline(tlArgs* args) {
    return args->size - (args->spec & 3);
}

static inline tlHandle tlArgsFnInline(tlArgs* args) {
    return args->fn;
}

static inline tlHandle tlArgsTargetInline(tlArgs* args) {
    if (args->spec & 2) return args->data[0];
    return null;
}

static inline tlHandle tlArgsMethodInline(tlArgs* args) {
    if (args->spec & 2) return args->data[1];
    return null;
}

static inline tlList* tlArgsNamesInline(tlArgs* args) {
    if (args->spec & 1) return args->data[args->spec & 2];
    return null;
}

void tlArgsDump(tlArgs* args);

tlList* tlArgsList(tlArgs* args);

bool tlArgsIsSetter(tlArgs* args);
tlList* tlArgsNames(tlArgs* args);
tlArgs* tlArgsFrom(tlArgs* call, tlHandle fn, tlSym method, tlHandle target);
tlArgs* tlArgsFromPrepend1(tlArgs* call, tlHandle fn, tlSym method, tlHandle target, tlHandle arg1);

void tlArgsMakeSetter_(tlArgs* args);

tlHandle tlInvoke(tlTask* task, tlArgs* call);

void args_init();

#endif
