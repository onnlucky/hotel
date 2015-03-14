#ifndef _code_h_
#define _code_h_

#include "tl.h"
#include "env.h"

enum {
    OP_END = 0,

    // 0b1100_0000
    OP_TRUE = 0xC0, OP_FALSE, OP_NULL, OP_UNDEF, OP_INT,      // literals (0-4)
    OP_SYSTEM, OP_MODULE, OP_GLOBAL,                          // access to global data (5-7)
    OP_ENVARG, OP_ENV, OP_ARG, OP_LOCAL,                      // access local data (8-11)
    OP_ARGS, OP_ENVARGS, OP_THIS, OP_ENVTHIS,                 // access to args and this (12-15)
    OP_BIND, OP_STORE, OP_RSTORE,                             // bind closures, store into locals (16-18)
    OP_INVOKE,                                                // invoke calls (19)
    OP_CERR,                                                  // end of ccall (conditional) sequence (20)
    OP_VGET, OP_VSTORE, OP_VRSTORE,                           // 21-23, get and set mutables in local env
    OP_EVGET, OP_EVSTORE, OP_EVRSTORE,                        // 24-26, get and set mutables in parent env
    // upto 31 instructions possible

    // 0b1110_0000
    OP_MCALL  = 0xE0, OP_FCALL, OP_BCALL, OP_CCALL, OP_SCALL, // building calls (method, function, binary op, conditional, setters)
    OP_MCALLN = 0xF0, OP_FCALLN, OP_BCALLN,                   // building calls with named arguments
    OP_MCALLS = 0xE8, OP_MCALLNS = 0xF8,                      // safe method calls foo?bar

    OP_LAST
};

const char* op_name(uint8_t op);

TL_REF_TYPE(tlBDebugInfo);
TL_REF_TYPE(tlBCode);
TL_REF_TYPE(tlBClosure);
TL_REF_TYPE(tlBLazy);
TL_REF_TYPE(tlBLazyData);
TL_REF_TYPE(tlBSendToken);

struct tlBDebugInfo {
    tlHead head;
    tlString* name; // name of function
    tlInt line; // line of function
    tlInt offset;
    tlString* text;
    tlList* lines; // byte positions of newlines in source file
    tlList* pos;   // byte positions of OP_GLOBAL and CALLs in source file
};

struct tlBCode {
    tlHead head;
    tlBModule* mod; // back reference to module it belongs

    tlBDebugInfo* debuginfo;
    tlList* argspec;    // a list representing how the arguments were defined [[name, default, @bool lazy]]
    tlList* localnames; // a list of names for each local
    tlList* localvars;  // if not null, contains true for for locals that are mutable

    int locals;
    int calldepth;
    int size;
    const uint8_t* code;
};

struct tlBClosure {
    tlHead head;
    tlBCode* code;
    tlEnv* env;
};

typedef struct tlCodeFrame tlCodeFrame;

typedef struct CallEntry { bool safe; bool ccall; bool bcall; int at; tlArgs* call; } CallEntry;
struct tlCodeFrame {
    tlFrame frame;    // TODO move resumecb into tlCodeFrameKind
    tlEnv* locals;    // locals->args
    tlHandle handler; // stack unwind handler

    // save/restore
    bool lazy;    // if this frame is evaluating a lazy invoke
    int8_t stepping; // if we are stepping
    int8_t bcall; // if this frame is evaluating part of a operator invoke
    int pc;
    tlArgs* invoke; // current invoke, here for bcalls, TODO remove by optimizing
    CallEntry calls[];
};

bool tlCodeFrameIs(tlHandle v);
tlCodeFrame* tlCodeFrameAs(tlHandle v);
tlCodeFrame* tlCodeFrameCast(tlHandle v);

tlEnv* tlCodeFrameEnv(tlFrame* frame);

int tlBCodePosOpsForPc(tlBCode* code, int pc);
tlHandle tlBCallGetExtra(tlArgs* call, int at, tlBCode* code);

void bcode_init();

#endif
