enum {
    OP_END = 0,

    // 0b1100_0000
    OP_TRUE = 0xC0, OP_FALSE, OP_NULL, OP_UNDEF, OP_INT,      // literals (0-4)
    OP_SYSTEM, OP_MODULE, OP_GLOBAL,                          // access to global data (5-7)
    OP_ENVARG, OP_ENV, OP_ARG, OP_LOCAL,                      // access local data (8-11)
    OP_ARGS, OP_ENVARGS, OP_THIS, OP_ENVTHIS,                 // access to args and this (12-15)
    OP_BIND, OP_STORE, OP_RSTORE,                             // bind closures, store into locals (16-18)
    OP_INVOKE,                                                // invoke calls (19)
    OP_CERR,                                                  // end of pcall sequence (20)
    OP_VGET, OP_VSTORE, OP_VRSTORE,                           // 21-23, get and set mutables in local env
    OP_EVGET, OP_EVSTORE, OP_EVRSTORE,                        // 24-26, get and set mutables in parent env
    // upto 31 instructions possible

    // 0b1110_0000
    OP_MCALL  = 0xE0, OP_FCALL, OP_BCALL, OP_CCALL,           // building calls
    OP_MCALLN = 0xF0, OP_FCALLN, OP_BCALLN,                   // building calls with named arguments
    OP_MCALLS = 0xE8, OP_MCALLNS = 0xF8,                      // safe method calls foo?bar

    OP_LAST
};

const char* op_name(uint8_t op);
