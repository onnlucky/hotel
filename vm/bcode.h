enum {
    OP_END = 0,

    // 0b1100_0000
    OP_TRUE = 0xC0, OP_FALSE, OP_NULL, OP_UNDEF, OP_INT,        // literals
    OP_SYSTEM, OP_MODULE, OP_GLOBAL,                            // access to global data
    OP_ENV, OP_ENVARG, OP_LOCAL, OP_ARG,                        // access local data
    OP_BIND, OP_STORE, OP_RSTORE, OP_INVOKE,                    // bind closures, store into locals, invoke calls
    // 16 more instructions available

    // 0b1110_0000
    OP_MCALL  = 0xE0, OP_FCALL, OP_BCALL,                       // building calls
    OP_MCALLN = 0xF0, OP_FCALLN, OP_BCALLN,                     // building calls with named arguments

    OP_LAST
};

const char* op_name(uint8_t op);
