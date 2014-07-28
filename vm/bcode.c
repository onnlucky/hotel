#include "bcode.h"
#define HAVE_DEBUG 1
#include "trace-off.h"

const char* op_name(uint8_t op) {
    switch (op) {
        case OP_END: return "END";
        case OP_TRUE: return "TRUE";
        case OP_FALSE: return "FALSE";
        case OP_NULL: return "NULL";
        case OP_UNDEF: return "UNDEF";
        case OP_INT: return "INT";
        case OP_SYSTEM: return "SYSTEM";
        case OP_MODULE: return "MODULE";
        case OP_GLOBAL: return "GLOBAL";
        case OP_ENV: return "ENV";
        case OP_ENVARG: return "ENVARG";
        case OP_LOCAL: return "LOCAL";
        case OP_ARG: return "ARG";
        case OP_BIND: return "BIND";
        case OP_STORE: return "STORE";
        case OP_RSTORE: return "RESULT";
        case OP_INVOKE: return "INVOKE";
        case OP_MCALL: return "MCALL";
        case OP_FCALL: return "FCALL";
        case OP_BCALL: return "BCALL";
        case OP_MCALLN: return "MCALLN";
        case OP_FCALLN: return "FCALLN";
        case OP_BCALLN: return "BCALLN";
    }
    return "<error>";
}

TL_REF_TYPE(tlBModule);
TL_REF_TYPE(tlBDebugInfo);
TL_REF_TYPE(tlBCode);
TL_REF_TYPE(tlBEnv);
TL_REF_TYPE(tlBClosure);
TL_REF_TYPE(tlBLazy);
TL_REF_TYPE(tlBLazyData);
TL_REF_TYPE(tlBSendToken);

typedef struct tlBFrame tlBFrame;

INTERNAL tlHandle resumeBFrame(tlFrame* _frame, tlHandle res, tlHandle throw);
tlDebugger* tlDebuggerFor(tlTask* task);
bool tlDebuggerStep(tlDebugger* debugger, tlTask* task, tlBFrame* frame);

// against better judgement, modules are linked by mutating, or copying if already linked
struct tlBModule {
    tlHead head;
    tlString* name; // name of this module, usually filename
    tlList* data;   // list of constants
    tlList* links;  // names of all global things referenced
    tlList* linked; // when linked, resolved values for all globals
    tlBCode* body;  // starting point; will reference linked and data
};

struct tlBDebugInfo {
    tlHead head;
    tlString* name;
    tlInt offset;
    tlString* text;
    tlList* pos;
};

struct tlBCode {
    tlHead head;
    tlBModule* mod; // back reference to module it belongs

    tlBDebugInfo* debuginfo;
    tlList* argspec;
    tlList* localnames;

    int locals;
    int calldepth;
    int size;
    const uint8_t* code;
};

struct tlBCall {
    tlHead head;
    int size;
    tlHandle target;
    tlHandle msg;
    tlHandle fn;
    tlList* names;
    tlHandle args[];
};

struct tlBEnv {
    tlHead head;
    tlBEnv* parent;
    tlBCall* args;
    tlList* names;
    tlHandle data[];
};

struct tlBClosure {
    tlHead head;
    tlBCode* code;
    tlBEnv* env;
};

typedef struct CallEntry { int at; tlBCall* call; } CallEntry;
struct tlBFrame {
    tlFrame frame;
    tlBCall* args;

    // mutable! but only by addition ...
    tlBEnv* locals;
    tlHandle handler; // stack unwind handler

    // save/restore
    int pc;
    CallEntry calls[];
};

struct tlBLazy {
    tlHead head;
    tlBCall* args;
    tlBEnv* locals;
    int pc; // where the actuall call is located in the code
};
struct tlBLazyData {
    tlHead head;
    tlHandle data;
};
struct tlBSendToken {
    tlHead head;
    tlSym method;
};

static tlKind _tlBModuleKind = { .name = "BModule" };
tlKind* tlBModuleKind;

static tlKind _tlBDebugInfoKind = { .name = "BDebugInfo" };
tlKind* tlBDebugInfoKind;

static tlKind _tlBCodeKind = { .name = "BCode" };
tlKind* tlBCodeKind;

static tlKind _tlBCallKind = { .name = "BCall" };
tlKind* tlBCallKind;

static tlKind _tlBEnvKind = { .name = "BEnv" };
tlKind* tlBEnvKind;

static tlKind _tlBClosureKind = { .name = "BClosure" };
tlKind* tlBClosureKind;

static tlKind _tlBLazyKind = { .name = "BLazy" };
tlKind* tlBLazyKind;

static tlKind _tlBLazyDataKind = { .name = "BLazyData" };
tlKind* tlBLazyDataKind;

static tlKind _tlBSendTokenKind = { .name = "BSendToken" };
tlKind* tlBSendTokenKind;

tlBModule* tlBModuleNew(tlString* name) {
    tlBModule* mod = tlAlloc(tlBModuleKind, sizeof(tlBModule));
    mod->name = name;
    return mod;
}
tlBDebugInfo* tlBDebugInfoNew() {
    tlBDebugInfo* info = tlAlloc(tlBDebugInfoKind, sizeof(tlBDebugInfo));
    return info;
}
tlBCode* tlBCodeNew(tlBModule* mod) {
    tlBCode* code = tlAlloc(tlBCodeKind, sizeof(tlBCode));
    code->mod = mod;
    return code;
}
tlBCall* tlBCallNew(int size) {
    tlBCall* call = tlAlloc(tlBCallKind, sizeof(tlBCall) + sizeof(tlHandle) * size);
    call->size = size;
    return call;
}
tlBEnv* tlBEnvNew(tlList* names, tlBEnv* parent) {
    tlBEnv* env = tlAlloc(tlBEnvKind, sizeof(tlBEnv) + sizeof(tlHandle) * tlListSize(names));
    env->names = names;
    env->parent = parent;
    return env;
}
tlString* tlBCodeName(tlBCode* code) {
    if (code->debuginfo) return code->debuginfo->name;
    return null;
}
tlBClosure* tlBClosureNew(tlBCode* code, tlBEnv* env) {
    tlBClosure* fn = tlAlloc(tlBClosureKind, sizeof(tlBClosure));
    fn->code = code;
    fn->env = env;
    return fn;
}

bool tlBFrameIs(tlHandle v) {
    return tlFrameIs(v) && tlFrameAs(v)->resumecb == resumeBFrame;
}
tlBFrame* tlBFrameAs(tlHandle v) {
    assert(tlBFrameIs(v));
    return (tlBFrame*)v;
}
tlBFrame* tlBFrameCast(tlHandle v) {
    if (!tlBFrameIs(v)) return null;
    return (tlBFrame*)v;
}

tlBFrame* tlBFrameNew(tlBCall* args) {
    int calldepth = tlBClosureAs(args->fn)->code->calldepth;
    tlBFrame* frame = tlFrameAlloc(resumeBFrame, sizeof(tlBFrame) + sizeof(CallEntry) * calldepth);
    frame->pc = -1;
    frame->args = args;
    return frame;
}
tlBLazy* tlBLazyNew(tlBCall* args, tlBEnv* locals, int pc) {
    assert(args);
    assert(pc > 0);
    tlBLazy* lazy = tlAlloc(tlBLazyKind, sizeof(tlBLazy));
    lazy->args = args;
    lazy->locals = locals;
    lazy->pc = pc;
    return lazy;
}
tlBLazyData* tlBLazyDataNew(tlHandle data) {
    tlBLazyData* lazy = tlAlloc(tlBLazyDataKind, sizeof(tlBLazyData));
    lazy->data = data;
    if (!data) lazy->data = tlNull;
    return lazy;
}

tlBSendToken* tlBSendTokenNew(tlSym method) {
    tlBSendToken* token = tlAlloc(tlBSendTokenKind, sizeof(tlBSendToken));
    token->method = method;
    return token;
}

bool tlBCallIsLazy(const tlBCall* call, int arg) {
    arg -= 2; // 0 == target; 1 == fn; 2 == arg[0]
    if (arg < 0 || !call || arg >= call->size) return false;
    if (tlBClosureIs(call->fn)) {
        // TODO this ignores FCALLN names
        tlHandle spec = tlListGet(tlBClosureAs(call->fn)->code->argspec, arg);
        if (!spec || spec == tlNull) return false;
        return tlTrue == tlListGet(tlListAs(spec), 2); // 0=name 1=default 2=lazy
    }
    // for old style interpreter compatibility
    // TODO this ignores FCALLN names
    if (tlClosureIs(call->fn)) {
        tlCode* code = tlClosureAs(call->fn)->code;
        tlList* names = code->argnames;
        tlObject* defaults = code->argdefaults;
        if (!defaults) return false;
        tlSym name = tlListGet(names, arg);
        if (!name) return false;
        tlHandle d = tlObjectGetSym(defaults, name);
        if (tlThunkIs(d) || d == tlThunkNull) return true;
        return false;
    }
    return false;
}

void tlBCallAdd_(tlBCall* call, tlHandle o, int arg) {
    assert(arg >= 0);
    if (arg == 0) {
        trace("%s.target <- %s", tl_str(call), tl_str(o));
        assert(!call->target);
        call->target = o;
        return;
    }
    if (arg == 1) {
        trace("%s.fn <- %s", tl_str(call), tl_str(o));
        assert(!call->fn || tlStringIs(call->fn));
        call->fn = o;
        return;
    }
    arg -= 2;
    assert(arg < call->size);
    trace("%s[%d] <- %s", tl_str(call), arg, tl_str(o));
    assert(!call->args[arg]);
    call->args[arg] = o;
}
void tlBCallSetNames_(tlBCall* call, tlList* names) {
    call->names = names;
}
tlHandle tlBCallGet(tlBCall* call, int at) {
    if (at < 0 || at >= call->size) return null;
    return call->args[at];
}
tlHandle tlBCallTarget(tlBCall* call) {
    return call->target;
}
tlHandle tlBCallGetFn(tlBCall* call) {
    return call->fn;
}

int tlBCallNameIndex(tlBCall* call, tlString* name) {
    for (int i = 0; i < call->size; i++) {
        tlHandle v = tlListGet(call->names, i);
        if (tlStringIs(v) && tlStringEquals(tlStringAs(v), name)) return i;
    }
    return -1;
}

int tlBCallGetUnnamedIndex(tlBCall* call, int skipped) {
    trace("get unnamed: %d", skipped);
    int i = 0;
    for (; i < call->size; i++) {
        if (tlStringIs(tlListGet(call->names, i))) continue;
        if (!skipped) break;
        skipped--;
    }
    return i;
}

tlHandle tlBCallGetExtra(tlBCall* call, int at, tlBCode* code) {
    tlList* argspec = tlListAs(tlListGet(code->argspec, at));
    tlString* name = tlStringCast(tlListGet(argspec, 0));
    trace("ARG(%d)=%s", at, tl_str(name));
    if (tlStringEquals(name, tlSTR("this"))) {
        return tlBCallTarget(call);
    }
    if (call->names) {
        int index = tlBCallNameIndex(call, name);
        if (index >= 0) return call->args[index];

        int skipped = 0;
        for (int i = 0; i < at; i++) {
            tlString* name = tlStringCast(tlListGet(tlListGet(code->argspec, i), 0));
            if (tlBCallNameIndex(call, name) < 0) skipped++;
        }

        at = tlBCallGetUnnamedIndex(call, skipped);
    }

    tlHandle v = tlBCallGet(call, at);
    if (v) return v;
    v = tlListGet(argspec, 1);
    bool lazy = tl_bool(tlListGet(argspec, 2));
    if (lazy) return tlBLazyDataNew(v);
    if (!v) return tlNull;
    return v;
}

tlObject* tlBEnvLocalObject(tlFrame* frame) {
    tlBEnv* env = tlBFrameAs(frame)->locals;
    tlList* names = env->names;
    tlObject* map = tlObjectNew(0);
    for (int i = 0; i < tlListSize(names); i++) {
        if (!names) continue;
        tlHandle name = tlListGet(names, i);
        tlHandle v = env->data[i];
        trace("env locals: %s=%s", tl_str(name), tl_str(v));
        if (!name) continue;
        if (!v) continue;
        map = tlObjectSet(map, tlSymAs(name), env->data[i]);
    }

    tlBCall* args = env->args;
    tlList* argspec = tlBClosureAs(args->fn)->code->argspec;
    for (int i = 0; i < tlListSize(argspec); i++) {
        tlHandle name = tlListGet(tlListGet(argspec, i), 0);
        tlHandle v = tlBCallGetExtra(args, i, tlBClosureAs(args->fn)->code);
        trace("args locals: %s=%s", tl_str(name), tl_str(v));
        if (!name) continue;
        if (!v) continue;
        map = tlObjectSet(map, tlSymAs(name), v);
    }

    assert(map != tlObjectEmpty());
    map = tlObjectToObject_(map);
    assert(tlObjectIs(map));
    return map;
}

tlHandle tlBEnvGet(tlBEnv* env, int at) {
    assert(env);
    assert(at >= 0 && at <= tlListSize(env->names));
    return env->data[at];
}
tlBEnv* tlBEnvGetParentAt(tlBEnv* env, int depth) {
    if (depth == 0) return env;
    if (env == null) return null;
    return tlBEnvGetParentAt(env->parent, depth - 1);
}
tlHandle tlBEnvArgGet(tlBEnv* env, int at) {
    assert(env);
    assert(env->args);
    assert(at >= 0);
    return tlBCallGetExtra(env->args, at, tlBClosureAs(env->args->fn)->code);
}

#define FAIL(r) do{ *error = r; return null; }while(0)

static tlHandle decodelit(uint8_t b);
static int dreadsize(const uint8_t** code2) {
    const uint8_t* code = *code2;
    int res = 0;
    while (true) {
        uint8_t b = *code; code++;
        if (b < 0x80) { *code2 = code; return (res << 7) | b; }
        res = (res << 6) | (b & 0x3F);
    }
}
static inline int pcreadsize(const uint8_t* ops, int* pc) {
    int res = 0;
    while (true) {
        uint8_t b = ops[(*pc)++];
        if (b < 0x80) { return (res << 7) | b; }
        res = (res << 6) | (b & 0x3F);
    }
}
static tlHandle dreadref(const uint8_t** code, const tlList* data) {
    uint8_t b = **code;
    if (b > 0xC0) {
        *code = *code + 1;
        return decodelit(b);
    }
    int r = dreadsize(code);
    if (data) return tlListGet(data, r);
    return null;
}
static inline tlHandle pcreadref(const uint8_t* ops, int* pc, const tlList* data) {
    uint8_t b = ops[*pc];
    if (b > 0xC0) {
        (*pc)++;
        return decodelit(b);
    }
    int r = pcreadsize(ops, pc);
    if (data) return tlListGet(data, r);
    return null;
}

// 11.. ....
static inline tlHandle decodelit(uint8_t b1) {
    int lit = b1 & 0x3F;
    switch (lit) {
        case 0: return tlNull;
        case 1: return tlFalse;
        case 2: return tlTrue;
        case 3: return tlStringEmpty();
        case 4: return tlListEmpty();
        case 5: return tlObjectEmpty();
    }
    return tlINT(lit - 8);
}

// 0... ....
// 10.. .... 0... ....
static int decoderef2(tlBuffer* buf, uint8_t b1) {
    if ((b1 & 0x80) == 0x00) return b1 & 0x7F;
    int res = b1 & 0x3F;
    for (int i = 0; i < 4; i++) {
        b1 = tlBufferReadByte(buf);
        if ((b1 & 0x80) == 0x00) {
            res = (res << 7) + (b1 & 0x7F);
            break;
        }
        assert((b1 & 0x80) == 0x80);
        res = (res << 6) + (b1 & 0x3F);
    }
    return res;
}

tlHandle readref(tlBuffer* buf, tlList* data, int* val) {
    uint8_t b1 = tlBufferReadByte(buf);

    if ((b1 & 0xC0) == 0xC0) return decodelit(b1);

    int at = decoderef2(buf, b1);
    if (val) *val = at;
    if (data) return tlListGet(data, at);
    return null;
}

static int readsize(tlBuffer* buf) {
    return decoderef2(buf, tlBufferReadByte(buf));
}

tlString* readString(tlBuffer* buf, int size, const char** error) {
    trace("string: %d", size);
    if (tlBufferSize(buf) < size) FAIL("not enough data");
    char *data = malloc_atomic(size + 1);
    tlBufferRead(buf, data, size);
    data[size] = 0;
    return tlStringFromTake(data, size);
}

tlList* readList(tlBuffer* buf, int size, tlList* data, const char** error) {
    trace("list: %d", size);
    tlList* list = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlHandle v = readref(buf, data, null);
        if (!v) FAIL("unable to read value for list");
        trace("LIST: %d: %s", i, tl_str(v));
        tlListSet_(list, i, v);
    }
    return list;
}

tlObject* readMap(tlBuffer* buf, int size, tlList* data, const char** error) {
    trace("map: %d (%d)", size, tlBufferSize(buf));
    tlList* pairs = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlHandle key = readref(buf, data, null);
        if (!key) FAIL("unable to read key for map");
        tlHandle v = readref(buf, data, null);
        if (!v) FAIL("unable to read value for map");
        trace("MAP: %s: %s", tl_str(key), tl_str(v));
        tlListSet_(pairs, i, tlListFrom2(tlSymFromString(key), v));
    }
    return tlObjectFromPairs(pairs);
}

tlBCode* readbytecode(tlBuffer* buf, tlList* data, tlBModule* mod, int size, const char** error) {
    if (tlBufferSize(buf) < size) FAIL("not enough data");
    int start = tlBufferSize(buf);

    tlBCode* bcode = tlBCodeNew(mod);
    tlHandle name = readref(buf, data, null);
    if (!tlStringIs(name) || tlNullIs(name)) FAIL("code[1] must be the name (string|null)");

    tlHandle argspec = readref(buf, data, null);
    if (!tlListIs(argspec)) FAIL("code[2] must be the argspec (list)");
    bcode->argspec = tlListAs(argspec);

    tlHandle localnames = readref(buf, data, null);
    if (!tlListIs(localnames)) FAIL("code[3] must be the localnames (list)");
    bcode->localnames = tlListAs(localnames);
    trace(" %s -- %s %s", tl_str(tlBCodeName(bcode)), tl_str(bcode->argspec), tl_str(bcode->localnames));

    tlHandle debuginfo = readref(buf, data, null);
    if (!tlObjectIs(debuginfo) || tlNullIs(debuginfo)) FAIL("code[4] must be debuginfo (object|null)");

    tlBDebugInfo* info = tlBDebugInfoNew();
    info->name = tlStringAs(name);
    if (tlObjectIs(debuginfo)) {
        info->offset = tlIntAs(tlObjectGet(debuginfo, tlSYM("offset")));
        info->text = tlStringAs(tlObjectGet(debuginfo, tlSYM("text")));
        info->pos = tlListAs(tlObjectGet(debuginfo, tlSYM("pos")));
    } else {
        info->offset = tlINT(0);
        info->text = tlStringEmpty();
        info->pos = tlListEmpty();
    }
    bcode->debuginfo = info;

    // we don't want above data in the bcode ... so skip it
    size -= start - tlBufferSize(buf);
    if (size < 0) FAIL("not enough data");
    uint8_t *code = malloc_atomic(size);
    tlBufferRead(buf, (char*)code, size);
    if (code[size - 1] != 0) FAIL("missing END opcode");
    bcode->size = size;
    bcode->code = code;

    trace("code: %s", tl_str(bcode));
    return bcode;
}

tlHandle readsizedvalue(tlBuffer* buf, tlList* data, tlBModule* mod, uint8_t b1, const char** error) {
    int size = readsize(buf);
    assert(size > 0);
    if (size > 100000) FAIL("value too large");
    switch (b1) {
        case 0xE0: return readString(buf, size, error);
        case 0xE1: return readList(buf, size, data, error);
        case 0xE2: // set
            trace("set %d", size);
            FAIL("unimplemented: set");
        case 0xE3: return readMap(buf, size, data, error);
        case 0xE4: // raw
            trace("raw %d", size);
            FAIL("unimplemented: raw");
        case 0xE5: // bytecode
            trace("bytecode %d", size);
            return readbytecode(buf, data, mod, size, error);
        case 0xE6: // bignum
            trace("bignum %d", size);
            FAIL("unimplemented: bignum");
        default:
            FAIL("not a sized value");
    }
    fatal("not reached");
    return null;
}

tlHandle readvalue(tlBuffer* buf, tlList* data, tlBModule* mod, const char** error) {
    if (tlBufferSize(buf) < 1) FAIL("not enough data");
    uint8_t b1 = tlBufferReadByte(buf);
    uint8_t type = b1 & 0xE0;
    uint8_t size = b1 & 0x1F;

    switch (type) {
        case 0x00: return readString(buf, size, error);
        case 0x20: return readList(buf, size, data, error);
        case 0x40: // short set
            trace("set %d", size);
            FAIL("not implemented: set");
        case 0x60: return readMap(buf, size, data, error);
        case 0x80: // short raw
            trace("raw %d", size);
            FAIL("not implemented: raw");
        case 0xA0: // short bytecode
            trace("short bytecode: %d", size);
            return readbytecode(buf, data, mod, size, error);
        case 0xC0: { // short size number
            if (size == 8) {
                trace("float size 4");
                uint32_t n = 0;
                for (int i = 0; i < 4; i++) {
                    n = (n << 8) | tlBufferReadByte(buf);
                }
                trace("float: %x %f", n, *(float*)&n);
                return tlFLOAT(*(float*)&n);
            }
            if (size == 9) {
                trace("float size 8");
                uint64_t n = 0;
                for (int i = 0; i < 8; i++) {
                    n = (n << 8) | tlBufferReadByte(buf);
                }
                trace("float: %llx %f", n, *(double*)&n);
                return tlFLOAT(*(double*)&n);
            }
            if (size > 3) FAIL("bad number or not implemented");
            trace("int size %d", size + 1);
            int val = 0;
            for (int i = 0; i < size; i++) {
                val = (val << 8) | tlBufferReadByte(buf);
            }
            // size is off by one, but last byte is 7 bits + sign
            int last = tlBufferReadByte(buf);
            val = (val << 7) | (last >> 1);
            if (last & 0x1) val = -val;
            trace("read int: %d", val);
            return tlINT(val);
        }
        case 0xE0: // sized types
            return readsizedvalue(buf, data, mod, b1, error);
        default:
            FAIL("unable to read value");
    }
    fatal("not reached");
    return null;
}


tlHandle deserialize(tlBuffer* buf, tlBModule* mod, const char** error) {
    trace("parsing: %d", tlBufferSize(buf));

    if (tlBufferReadByte(buf) != 't') return null;
    if (tlBufferReadByte(buf) != 'l') return null;
    if (tlBufferReadByte(buf) != '0') return null;
    if (tlBufferReadByte(buf) != '1') return null;

    int size = 0;
    tlHandle direct = readref(buf, null, &size);
    if (direct) {
        assert(size == 0);
        trace("tl01 - %s", tl_str(direct));
        return direct;
    }
    trace("tl01 - %d", size);
    if (size > 1000) FAIL("too many values");

    tlHandle v = null;
    tlList* data = tlListNew(size);
    for (int i = 0; i < size; i++) {
        v = readvalue(buf, data, mod, error);
        if (!v) FAIL("unable to read value");
        trace("%d: %s", i, tl_str(v));
        tlListSet_(data, i, v);
    }
    if (mod) {
        mod->data = data;
        tlHandle links = tlListGet(data, size - 2);
        tlHandle body = tlListGet(data, size - 1);
        if (!tlListIs(links)) FAIL("modules end with linker list as second last value");
        if (!tlBCodeIs(body)) FAIL("modules end with code as last value");
        mod->links = tlListAs(links);
        mod->body = tlBCodeAs(body);
    }
    return v;
}

#include "trace-off.h"
static void disasm(tlBCode* bcode) {
    assert(bcode->mod);
    const uint8_t* ops = bcode->code;
    tlList* data = bcode->mod->data;
    tlList* links = bcode->mod->links;
    tlList* linked = bcode->mod->linked;

    print("<code>");
    print(" name: %s::%s", tl_str(bcode->mod->name), tl_str(tlBCodeName(bcode)));
    print(" <args>");
    for (int i = 0;; i++) {
        tlHandle spec = tlListGet(bcode->argspec, i);
        if (!spec) break;
        print("   name=%s, default=%s, lazy=%s", tl_str(tlListGet(spec, 0)), tl_str(tlListGet(spec, 1)), tl_str(tlListGet(spec, 2)));
    }
    print(" </args>");
    print(" <locals>");
    for (int i = 0;; i++) {
        tlHandle name = tlListGet(bcode->localnames, i);
        if (!name) break;
        print("  %d: %s", i, tl_str(name));
    }
    print(" </locals>");
    int pc = 0;
    int r = 0; int r2 = 0; tlHandle o = null;
    while (true) {
        int opc = pc;
        uint8_t op = ops[pc++];
        switch (op) {
            case OP_END: goto exit;
            case OP_TRUE: case OP_FALSE: case OP_NULL: case OP_UNDEF: case OP_INVOKE:
                print(" % 3d 0x%X %s", opc, op, op_name(op));
                break;
            case OP_INT: case OP_LOCAL: case OP_ARG:
            case OP_MCALL: case OP_FCALL: case OP_BCALL:
                r = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d", opc, op, op_name(op), r);
                break;
            case OP_MCALLN: case OP_FCALLN: case OP_BCALLN:
                r = pcreadsize(ops, &pc);
                o = pcreadref(ops, &pc, data);
                print(" % 3d 0x%X %s: %d n: %s", opc, op, op_name(op), r, tl_str(o));
                break;
            case OP_SYSTEM: case OP_MODULE:
                o = pcreadref(ops, &pc, data);
                print(" % 3d 0x%X %s: %s", opc, op, op_name(op), tl_str(o));
                break;
            case OP_GLOBAL:
                r = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %s(%s)", opc, op, op_name(op),
                        tl_str(tlListGet(links, r)), tl_str(tlListGet(linked, r)));
                break;
            case OP_ENV: case OP_ENVARG:
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d %d", opc, op, op_name(op), r, r2);
                break;
            case OP_BIND:
                o = pcreadref(ops, &pc, data);
                print(" % 3d 0x%X %s: %s", opc, op, op_name(op), tl_str(o));
                //if (tlBCodeIs(o)) disasm(tlBCodeAs(o));
                break;
            case OP_STORE:
                r = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d(%s) <-", opc, op, op_name(op), r, tl_str(tlListGet(bcode->localnames, r)));
                break;
            case OP_RSTORE:
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d(%s) <- %d", opc, op, op_name(op), r2, tl_str(tlListGet(bcode->localnames, r2)), r);
                break;
            default: print("OEPS: %d 0x%X %s", opc, op, op_name(op));
        }
    }
exit:;
    print("</code>");
}

void bpprint(tlHandle v) {
    if (tlBCodeIs(v)) {
        tlBCode* code = tlBCodeAs(v);
        disasm(code);
        return;
    }
    if (tlObjectIs(v)) {
        tlObject* map = tlObjectAs(v);
        print("{");
        for (int i = 0;; i++) {
            tlHandle v = tlObjectValueIter(map, i);
            if (!v) break;
            tlHandle k = tlObjectKeyIter(map, i);
            print("%s:", tl_str(k));
            bpprint(v);
        }
        print("}");
        return;
    }
    if (tlListIs(v)) {
        tlList* list = tlListAs(v);
        print("[");
        for (int i = 0;; i++) {
            tlHandle v = tlListGet(list, i);
            if (!v) break;
            bpprint(v);
        }
        print("]");
        return;
    }
    print("%s", tl_str(v));
}

// verify and fill in call depth and balancing, and use of locals
tlHandle tlBCodeVerify(tlBCode* bcode, const char** error) {
    int locals = tlListSize(bcode->localnames);
    int maxdepth = 0;
    int depth = 0;

    assert(bcode->mod);
    const uint8_t* code = bcode->code;
    tlList* data = bcode->mod->data;

    tlHandle v;
    int parent;
    int at;

    // print the code
    //disasm(bcode);

    while (true) {
        uint8_t op = *code; code++;
        switch (op) {
            case OP_END: goto exit;
            case OP_TRUE: break;
            case OP_FALSE: break;
            case OP_NULL: break;
            case OP_UNDEF: break;
            case OP_INT: dreadsize(&code); break;
            case OP_SYSTEM: dreadsize(&code); break;
            case OP_MODULE: dreadref(&code, data); break;
            case OP_GLOBAL: dreadsize(&code); break;
            case OP_ENV:
                parent = dreadsize(&code);
                at = dreadsize(&code);
                // TODO check if parent locals actually support these
                assert(parent >= 0);
                assert(at >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;
            case OP_ENVARG:
                parent = dreadsize(&code);
                at = dreadsize(&code);
                // TODO check if parent locals actually support these
                assert(parent >= 0);
                assert(at >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;
            case OP_LOCAL:
                at = dreadsize(&code);
                assert(at >= 0);
                if (at >= locals) FAIL("local out of range");
                break;
            case OP_ARG: dreadsize(&code); break;
            case OP_BIND:
                v = dreadref(&code, data);
                tlBCodeVerify(tlBCodeAs(v), error); // TODO pass in our locals somehow for ENV
                if (*error) return null;
                break;
            case OP_STORE:
                at = dreadsize(&code);
                assert(at >= 0);
                if (at >= locals) FAIL("local store out of range");
                break;
            case OP_RSTORE:
                dreadsize(&code);
                at = dreadsize(&code);
                assert(at >= 0);
                if (at >= locals) FAIL("local store out of range");
                break;

            case OP_INVOKE:
                if (depth <= 0) FAIL("invoke without call");
                if (depth > maxdepth) maxdepth = depth;
                depth--;
                break;
            case OP_MCALL:
            case OP_FCALL:
            case OP_BCALL:
                dreadsize(&code);
                depth++;
                break;
            case OP_MCALLN:
            case OP_FCALLN:
            case OP_BCALLN:
                dreadsize(&code);
                v = dreadref(&code, data);
                depth++;
                break;
            default:
                warning("%d - %s", op, op_name(op));
                FAIL("not an opcode");
        }
    }
exit:;
     if (depth != 0) FAIL("call without invoke");
     trace("verified; locals: %d%s, calldepth: %d, %s", locals, maxdepth, tl_str(bcode));
     bcode->locals = locals;
     bcode->calldepth = maxdepth;
     return tlNull;
}

#include "trace-off.h"
tlBModule* tlBModuleLink(tlBModule* mod, tlEnv* env, const char** error) {
    trace("linking: %p", mod);
    assert(!mod->linked);
    mod->linked = tlListNew(tlListSize(mod->links));
    for (int i = 0; i < tlListSize(mod->links); i++) {
        tlHandle name = tlListGet(mod->links, i);
        tlHandle v = tlEnvGet(env, tlSymFromString(name));
        if (!v) v = tlNull; // TODO tlUndef
        trace("linking: %s as %s", tl_str(name), tl_str(v));
        tlListSet_(mod->linked, i, v);
    }
    tlBCodeVerify(mod->body, error);
    if (*error) return null;
    return mod;
}

tlHandle beval(tlTask* task, tlBFrame* frame, tlBCall* args, int lazypc, tlBEnv* locals);

tlArgs* argsFromBCall(tlBCall* call) {
    tlObject* map = tlObjectEmpty();
    if (call->names) {
        for (int i = 0; i < call->size; i++) {
            tlHandle name = tlListGet(call->names, i);
            if (name && name != tlNull) {
                trace("ARGS: %s = %s", tl_str(name), tl_str(call->args[i]));
                map = tlObjectSet(map, tlSymAs(name), call->args[i]);
            }
        }
    }

    tlArgs* args = tlArgsNew(tlListNew(call->size - tlObjectSize(map)), map);
    tlArgsSetTarget_(args, call->target);
    tlArgsSetFn_(args, call->fn);
    tlArgsSetMsg_(args, call->msg);
    for (int i = 0, j = 0; i < call->size; i++) {
        if (call->names) {
            tlHandle name = tlListGet(call->names, i);
            if (name && name != tlNull) continue;
        }
        trace("ARGS: %d = %s", j, tl_str(call->args[i]));
        tlArgsSet_(args, j++, call->args[i]);
    }
    return args;
}

tlHandle tlInvoke(tlTask* task, tlBCall* call) {
    tlHandle fn = tlBCallGetFn(call);
    trace(" %s invoke %s.%s", tl_str(call), tl_str(call->target), tl_str(fn));
    if (call->target && tl_kind(call->target)->locked) {
        if (!tlLockIsOwner(tlLockAs(call->target), task)) return tlLockAndInvoke(call);
    }

    if (tlBSendTokenIs(fn) || tlNativeIs(fn) || tlClosureIs(fn)) {
        tlArgs* args = argsFromBCall(call);
        if (tlBSendTokenIs(fn)) {
            assert(tlArgsMsg(args) == tlBSendTokenAs(fn)->method);
            return tl_kind(call->target)->send(args);
        }
        if (tlNativeIs(fn)) return tlNativeKind->run(tlNativeAs(fn), args);
        return runClosure(tlClosureAs(fn), args);
    }
    if (tlBClosureIs(fn)) {
        return beval(task, null, call, 0, null);
    }
    if (tlBLazyIs(fn)) {
        tlBLazy* lazy = tlBLazyAs(fn);
        return beval(task, null, lazy->args, lazy->pc, lazy->locals);
    }
    if (tlBLazyDataIs(fn)) {
        return tlBLazyDataAs(fn)->data;
    }
    // if method call and target is set, field is already resolved and apparently not callable
    if (call->target && !tlCallableIs(fn)) {
        assert(fn);
        return fn;
    }
    // oldstyle hotel, include {call=(->)}
    if (tlCallableIs(fn)) {
        tlArgs* args = argsFromBCall(call);
        tlKind* kind = tl_kind(fn);
        if (kind->run) return kind->run(fn, args);
    }
    TL_THROW("'%s' not callable", tl_str(fn));
}

static tlBLazy* create_lazy(const uint8_t* ops, const int len, int* ppc, tlBCall* args, tlBEnv* locals) {
    // match CALLs with INVOKEs and wrap it for later execution
    // notice bytecode vs literals are such that OP_CALL cannot appear in literals
    int pc = *ppc;
    int start = pc - 2; // we re-interpret OP_CALL and SIZE when working on lazy; TODO - 2 is not always correct?
    int depth = 1;
    while (pc < len) {
        uint8_t op = ops[pc++];
        trace("SEARCHING: %d - %d - %s(%X)", depth, pc - 1, op_name(op), op);
        if (op == OP_INVOKE) {
            depth--;
            if (!depth) {
                *ppc = pc;
                trace("%d", start);
                return tlBLazyNew(args, locals, start);
            }
        } else if ((op & 0xE0) == 0xE0) { // OP_CALL mask
            trace("ADDING DEPTH: %X", op);
            depth++;
        }
    }
    fatal("bytecode invalid");
    return null;
}

// TODO this is not really needed, compiler or verifier can tell if a real locals needs to be realized
static void ensure_locals(tlHandle (**data)[], tlBEnv** locals, tlBCall* args, tlBClosure* fn) {
    if (*locals) {
        assert(&(*locals)->data == *data);
        return;
    }
    *locals = tlBEnvNew(fn->code->localnames, fn->env);
    (*locals)->args = args;
    int size = tlListSize(fn->code->localnames);
    for (int i = 0; i < size; i++) {
        (*locals)->data[i] = (**data)[i];
    }
    *data = &(*locals)->data; // switch backing store
}

// ensure both frame and locals exist or are created and switch the calls and locals backing stores
static void ensure_frame(CallEntry (**calls)[], tlHandle (**data)[], tlBFrame** frame, tlBEnv** locals, tlBCall* args) {
    if (*frame) {
        assert(*locals);
        assert(&(*locals)->data == *data);
        assert(&(*frame)->calls == *calls);
        return;
    }
    ensure_locals(data, locals, args, tlBClosureAs(args->fn));
    *frame = tlBFrameNew(args);
    (*frame)->locals = *locals;
    int calldepth = tlBClosureAs(args->fn)->code->calldepth;
    for (int i = 0; i < calldepth; i++) {
        (*frame)->calls[i] = (**calls)[i];
    }
    *calls = &(*frame)->calls; // switch backing store
}

static tlHandle bmethodResolve(tlHandle target, tlSym method) {
    trace("resolve method: %s.%s", tl_str(target), tl_str(method));
    if (tlObjectIs(target)) return objectResolve(target, method);
    tlKind* kind = tl_kind(target);
    if (kind->send) return tlBSendTokenNew(method);
    if (kind->klass) return objectResolve(kind->klass, method);
    return null;
}

tlHandle beval(tlTask* task, tlBFrame* frame, tlBCall* args, int lazypc, tlBEnv* lazylocals) {
    trace("task=%s frame=%s args=%s lazypc=%d lazylocals=%s", tl_str(task), tl_str(frame), tl_str(args), lazypc, tl_str(lazylocals));
    assert(task);
    if (!args) {
        assert(frame);
        args = frame->args;
        assert(args);
    }
    tlBClosure* closure = tlBClosureAs(args->fn);
    tlBCode* bcode = closure->code;
    tlBModule* mod = bcode->mod;
    tlList* data = mod->data;
    const uint8_t* ops = bcode->code;

    // program counter, call stack, locals; saved and restored into frame
    int pc = 0;
    CallEntry (*calls)[]; // floeps, c, are you high? ... a pointer to an array of CallEntry's
    tlHandle (*locals)[]; // idem; so we can use the c-stack as initial backing store, but can also use the frame

    // on stack storage if there is no frame yet
    CallEntry _calls[frame? 0 : bcode->calldepth];
    tlHandle _locals[(frame || lazylocals)? 0 : bcode->locals];
    if (!(frame || lazylocals)) for (int i = 0; i < bcode->locals; i++) _locals[i] = 0;

    int calltop = -1; // current call index in the call stack
    tlBCall* call = null; // current call, if any
    int arg = 0; // current argument to a call
    bool lazy = false; // if current call[arg] is lazy

    uint8_t op = 0; // current opcode
    tlHandle v = tlNull; // tmp result register

    // get current attached debugger, if any
    tlDebugger* debugger = tlDebuggerFor(task);

    // when we get here, it is because of the following reasons:
    // 1. we start a closure
    // 2. we start a lazy call
    // 3. we did an invoke, and paused the task, but now the result is ready; see OP_INVOKE
    // 4. we set a method name to a call, and resolving it paused the task; see end of this function
    // 5. we resumed from a debugger
    if (frame) { // 3 or 4 or 5
        assert(args == frame->args);
        pc = frame->pc;
        if (pc < 0) {
            pc = -pc;
            lazypc = 1;
            trace("restoring lazypc to 1");
        }
        calls = &frame->calls;
        lazylocals = frame->locals;
        locals = &lazylocals->data;
        v = task->value;

        // TODO we really need to know if we came from debugger or not ... just having a debugger is not good enough
        // plus, debuggers attach/deattach any time
        if (bcode->calldepth == 0 || !(*calls)[0].call) {
            trace("resume eval; %s %s; %s locals: %d call: %d/%d[%d]", tl_str(frame), tl_str(v),
                tl_str(bcode), bcode->locals, calltop, bcode->calldepth, arg);
            if (lazypc) goto resume;
            goto again;
        }

        // find current call, we mark non current call by setting sign bit
        assert(bcode->calldepth > 0); // cannot be zero, or we would never suspend
        for (calltop = 0; (*calls)[calltop].at < 0; calltop++);
        call = (*calls)[calltop].call;
        arg = (*calls)[calltop].at;
        assert(calltop >= 0 && calltop < bcode->calldepth);
        assert(call || calltop == 0);

        trace("resume eval; %s %s; %s locals: %d call: %d/%d[%d]", tl_str(frame), tl_str(v),
                tl_str(bcode), bcode->locals, calltop, bcode->calldepth, arg);
        if (debugger) goto again;
        goto resume; // for speed and code density
    } else if (lazypc) { // 2
        pc = lazypc;
        calls = &_calls;
        locals = &lazylocals->data;
        trace("lazy eval; %s locals: %d call: %d/%d",
                tl_str(bcode), bcode->locals, calltop, bcode->calldepth);
    } else { // 1
        calls = &_calls;
        locals = &_locals;
        trace("new eval; %s locals: %d call: %d/%d",
                tl_str(bcode), bcode->locals, calltop, bcode->calldepth);
        for (int i = 0; i < bcode->calldepth; i++) {
            (*calls)[i].at = 0;
            (*calls)[i].call = null;
        }
    }

    // if there is a debugger, always create a frame
    if (debugger) ensure_frame(&calls, &locals, &frame, &lazylocals, args);
    ensure_frame(&calls, &locals, &frame, &lazylocals, args);
    frame->frame.caller = tlTaskCurrentFrame(task); // not entirely correct ...
    tlTaskSetCurrentFrame(task, (tlFrame*)frame);

again:;
    if (debugger && frame->pc != pc) {
        task->value = v;
        frame->pc = pc;
        if (calltop >= 0) (*calls)[calltop].at = arg;
        if (!tlDebuggerStep(debugger, task, frame)) {
            //task->stack = (tlFrame*)frame;
            trace("pausing for debugger");
            return tlTaskPause((tlFrame*)frame);
        }
    }
    op = ops[pc++];
    if (!op) {
        assert(v);
        return v; // OP_END
    }
    assert(op & 0xC0);

    lazy = tlBCallIsLazy(call, arg);
    trace("LAZY? %d[%d] %s", calltop, arg - 2, lazy?"yes":"no");

    // is it a call
    if (op & 0x20) {
        int size = pcreadsize(ops, &pc);
        trace("%d call op: 0x%X %s - %d", pc, op, op_name(op), size);

        if (lazy) {
            trace("lazy call");
            ensure_locals(&locals, &lazylocals, args, closure);
            v = create_lazy(ops, bcode->size, &pc, args, lazylocals);
            tlBCallAdd_(call, v, arg++);
            goto again;
        }

        // create a new call
        if (calltop >= 0) {
            assert((*calls)[calltop].call == call);
            (*calls)[calltop].at = -(arg + 1); // store current arg, marked as not current
            trace("push call: %d: %d %s", calltop, arg, tl_str(call));
        }
        arg = 0;
        call = tlBCallNew(size);
        calltop++;
        assert(calltop >= 0 && calltop < bcode->calldepth);
        (*calls)[calltop] = (CallEntry){ .at = 0, .call = call };

        // load names if it is a named call
        if (op & 0x10) {
            int at = pcreadsize(ops, &pc);
            tlBCallSetNames_(call, tlListAs(tlListGet(data, at)));
            trace("set names: %s", tl_str(call->names));
        }
        // for non methods, skip target
        if (op & 0x0F) tlBCallAdd_(call, null, arg++);
        goto again;
    }

    trace("%d data op: 0x%X %s", pc, op, op_name(op));
    int at;
    int depth;

    switch (op) {
        case OP_INVOKE: {
            assert(call);
            assert(!lazy);
            assert(arg - 2 == call->size); // must be done with args here
            tlBCall* invoke = call;
            (*calls)[calltop].at = 0;
            (*calls)[calltop].call = null;
            calltop--;
            if (calltop >= 0) {
                call = (*calls)[calltop].call;
                arg = (*calls)[calltop].at;
                arg = (*calls)[calltop].at = -arg - 1; // mark as current again
                trace("pop call: %d: %d %s", calltop, arg, tl_str(call));
            } else {
                call = null;
                arg = 0;
            }
            trace("invoke: %s", tl_str(invoke));
            frame->pc = pc;
            v = tlInvoke(task, invoke);
            if (!v) { // task paused instead of actually doing work, suspend and return
                ensure_frame(&calls, &locals, &frame, &lazylocals, args);
                if (calltop >= 0) (*calls)[calltop].at = arg; // mark as current
                if (lazypc) frame->pc = -frame->pc; // mark frame as lazy
                return tlTaskPauseAttach(frame);
            }
            pc = frame->pc;
            break;
        }
        case OP_SYSTEM:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            // TODO this doesn't take care of blocks vs function vs methods
            if (tlStringEquals(v, tlSTR("args"))) {
                trace("syscall args: %s", tl_str(args));
                v = argsFromBCall(args);
                break;
            }
            if (tlStringEquals(v, tlSTR("this"))) {
                v = args->target;
                tlBEnv* env = lazylocals;
                trace("syscall this: %s (%s)", tl_str(args), tl_str(v));
                while (!v && env) {
                    env = env->parent;
                    if (env) v = env->args->target;
                }
                if (!v) v = tlNull;
                break;
            }
            fatal("syscall: %s", tl_str(v));
            break;
        case OP_MODULE:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            trace("data %s", tl_str(v));
            break;
        case OP_GLOBAL:
            at = pcreadsize(ops, &pc);
            v = tlListGet(mod->linked, at);
            trace("linked %s (%s)", tl_str(v), tl_str(tlListGet(mod->links, at)));
            break;
        case OP_ENV: {
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlBEnv* parent = tlBEnvGetParentAt(closure->env, depth);
            assert(parent);
            v = tlBEnvGet(parent, at);
            trace("env[%d][%d] -> %s", depth, at, tl_str(v));
            break;
        }
        case OP_ENVARG: {
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlBEnv* parent = tlBEnvGetParentAt(closure->env, depth);
            v = tlBEnvArgGet(parent, at);
            trace("envarg[%d][%d] -> %s", depth, at, tl_str(v));
            break;
        }
        case OP_ARG:
            at = pcreadsize(ops, &pc);
            v = tlBCallGetExtra(args, at, bcode);
            trace("arg[%d] %s", at, tl_str(v));
            break;
        case OP_LOCAL:
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            v = (*locals)[at];
            if (!v) v = tlNull; // TODO tlUndefined
            trace("local %d -> %s", at, tl_str(v));
            break;
        case OP_BIND:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            ensure_locals(&locals, &lazylocals, args, closure);
            v = tlBClosureNew(tlBCodeAs(v), lazylocals);
            trace("%d bind %s", pc, tl_str(v));
            break;
        case OP_STORE:
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            (*locals)[at] = tlFirst(v);
            trace("store %d <- %s", at, tl_str(v));
            break;
        case OP_RSTORE: {
            int rat = pcreadsize(ops, &pc);
            tlHandle res = tlResultGet(v, rat);
            at = pcreadsize(ops, &pc);
            (*locals)[at] = res;
            trace("result %d <- %s (%d %s)", at, tl_str(res), rat, tl_str(v));
            break;
        }
        case OP_TRUE: v = tlTrue; break;
        case OP_FALSE: v = tlFalse; break;
        case OP_NULL: v = tlNull; break;
        case OP_UNDEF: v = tlUndef(); break;
        case OP_INT: v = tlINT(pcreadsize(ops, &pc)); break;
        default: fatal("unknown op: %d (%s)", op, op_name(op));
    }

resume:;
    assert(v);
    if (!call) {
        if (lazypc) return v; // if we were evaulating a lazy call, and we are back at "top"
        goto again;
    }

    // if setting a lazy argument, wrap the data
    if (lazy) v = tlBLazyDataNew(v);

    // set the data to the call
    trace("load: %s[%d] = %s", tl_str(call), arg, tl_str(v));
    tlBCallAdd_(call, tlFirst(v), arg++);

    // resolve method at object ...
    if (arg == 2 && call->target && tlStringIs(call->fn)) {
        tlSym msg = tlSymFromString(call->fn);
        tlHandle method = bmethodResolve(call->target, msg);
        trace("method resolve: %s %s", tl_str(call->fn), tl_str(method));
        if (!method) TL_THROW("undefined");
        call->msg = msg;
        call->fn = method;
    }
    goto again;
}

INTERNAL tlHandle resumeBCall(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("running resume a call");
    if (throw) return null;
    tlBCall* call = tlBCallAs(res);
    return beval(tlTaskCurrent(), null, call, 0, null);
}

INTERNAL tlHandle handleBFrameThrow(tlBFrame* frame, tlHandle throw) {
    tlHandle handler = frame->handler;
    if (tlBClosureIs(handler)) {
        trace("invoke closure as exception handler: %s(%s)", tl_str(handler), tl_str(throw));
        tlBCall* call = tlBCallNew(1);
        call->fn = tlBClosureAs(handler);
        tlBCallAdd_(call, throw, 2);
        return beval(tlTaskCurrent(), null, call, 0, null);
    }
    if (tlCallableIs(handler)) {
        // operate with old style hotel
        trace("invoke callable as exception handler: %s(%s)", tl_str(handler), tl_str(throw));
        tlCall* call = tlCallNew(1, null);
        tlCallSetFn_(call, handler);
        tlCallSet_(call, 0, throw);
        return tlEval(call);
    }
    trace("returing handler value as handled: %s", tl_str(handler));
    return handler;
}

INTERNAL tlHandle resumeBFrame(tlFrame* _frame, tlHandle res, tlHandle throw) {
    trace("running resuming from a frame: %s", tl_str(res));
    tlBFrame* frame = tlBFrameAs(_frame);
    if (throw) {
        if (frame->handler) return handleBFrameThrow(frame, throw);
        return null;
    }
    if (!res) return null;
    tlTaskCurrent()->value = res;
    return beval(tlTaskCurrent(), frame, null, 0, null);
}

tlHandle beval_module(tlTask* task, tlBModule* mod) {
    tlBClosure* fn = tlBClosureNew(mod->body, null);
    tlBCall* call = tlBCallNew(0);
    call->fn = fn;
    return beval(task, null, call, 0, null);
}

INTERNAL tlHandle _Module_new(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    tlString* name = tlStringCast(tlArgsGet(args, 1));
    tlHandle* out = tlArgsGet(args, 2);
    tlBModule* mod = tlBModuleNew(name);
    const char* error = null;
    deserialize(buf, mod, &error);
    if (error) TL_THROW("invalid bytecode: %s", error);
    tlEnv* current = tlVmGlobalEnv(tlVmCurrent());
    if (out) current = tlEnvSet(current, tlSYM("out"), out);
    tlBModuleLink(mod, current, &error);
    if (error) TL_THROW("invalid bytecode: %s", error);
    return mod;
}

INTERNAL tlHandle _module_run(tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    tlList* as = tlListCast(tlArgsGet(args, 1));
    if (!as) as = tlListEmpty();
    tlTask* task = tlTaskCast(tlArgsGet(args, 2));

    int kw = 0;
    for (int i = 0; i < tlListSize(as); i++) {
        assert(tlListIs(tlListGet(as, i)));
        if (tlListSize(tlListGet(as, i)) == 2) {
            assert(tlStringIs(tlListGet(tlListGet(as, i), 1)));
            kw++;
        }
    }

    tlBClosure* fn = tlBClosureNew(mod->body, null);
    tlBCall* call = tlBCallNew(tlListSize(as));
    call->names = kw? tlListNew(tlListSize(as)) : null;
    call->fn = fn;
    for (int i = 0; i < tlListSize(as); i++) {
        call->args[i] = tlListGet(tlListGet(as, i), 0);
        if (kw) tlListSet_(call->names, i, tlListGet(tlListGet(as, i), 1));
    }

    if (!task) {
        return beval(tlTaskCurrent(), null, call, 0, null);
    }

    task->value = call;
    task->stack = tlFrameAlloc(resumeBCall, sizeof(tlFrame));
    assert(tlTaskIsDone(task));
    task->state = TL_STATE_INIT;
    tlTaskStart(task);
    return tlNull;
}

INTERNAL tlHandle _module_links(tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    return mod->links;
}

INTERNAL tlHandle _module_link(tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    tlList* links = tlListCast(tlArgsGet(args, 1));
    if (!mod) TL_THROW("expect a module as arg[1]");
    if (!links) TL_THROW("expect a list as arg[2]");
    if (tlListSize(links) != tlListSize(mod->links)) TL_THROW("arg[2].size != module.links.size");

    mod->linked = tlListNew(tlListSize(mod->links));
    for (int i = 0; i < tlListSize(mod->links); i++) {
        tlHandle v = tlListGet(links, i);
        trace("linking: %s as %s", tl_str(tlListGet(mod->links, i)), tl_str(v));
        tlListSet_(mod->linked, i, v);
    }

    return mod;
}

INTERNAL tlHandle __list(tlArgs* args) {
    tlList* list = tlListEmpty();
    for (int i = 0; i < tlArgsSize(args); i++) {
        list = tlListAppend(list, tlArgsGet(args, i));
    }
    return list;
}

INTERNAL tlHandle __object(tlArgs* args) {
    // TODO make faster ;)
    tlObject* o = tlObjectEmpty();
    for (int i = 0; i < tlArgsSize(args); i += 2) {
        o = tlObjectSet(o, tlArgsGet(args, i), tlArgsGet(args, i + 1));
    }
    return o;
}

INTERNAL tlHandle __map(tlArgs* args) {
    // TODO make faster ;)
    tlMap* map = tlMapEmpty();
    for (int i = 0; i < tlArgsSize(args); i += 2) {
        map = tlMapSet(map, tlArgsGet(args, i), tlArgsGet(args, i + 1));
    }
    return map;
}

INTERNAL tlHandle resume_breturn(tlFrame* _frame, tlHandle _res, tlHandle throw) {
    if (throw || !_res) return null;
    tlArgs* args = tlArgsAs(_res);

    int deep = tl_int(tlArgsGet(args, 0));
    trace("RETURN SCOPES: %d", deep);

    // TODO we should return our scoped function, not the first frame ...
    tlHandle res = tlNull;
    if (tlArgsSize(args) == 2) res = tlArgsGet(args, 1);
    if (tlArgsSize(args) > 2) res = tlResultFromArgsSkipOne(args);

    tlBFrame* scopeframe = tlBFrameAs(_frame->caller);
    assert(scopeframe->locals);
    tlBCall* target = tlBEnvGetParentAt(scopeframe->locals, deep)->args;

    tlFrame* frame = (tlFrame*)scopeframe;
    while (frame) {
        trace("%p", frame);
        if (tlBFrameIs(frame)) {
            tlBFrame* bframe = tlBFrameAs(frame);
            trace("found a bframe: %s{.pc=%d,.args=%s) env->args=%s", tl_str(bframe), bframe->pc, tl_str(bframe->args), tl_str(bframe->locals->args));
            // negative pc means a lazy frame, has same locals and bcall as target ... but we ignore it
            if (bframe->pc > 0 && bframe->args == target) {
                scopeframe = bframe;
                break;
            }
            // we keep track of lexically related frames, for when the to return to target is not on stack (captured blocks)
            if (bframe->locals == scopeframe->locals->parent) {
                scopeframe = tlBFrameAs(frame);
                trace("found a scope frame: %s.pc=%d (%d)", tl_str(frame), scopeframe->pc, deep);
            }
        }
        frame = frame->caller;
    }
    assert(scopeframe);
    return tlTaskStackUnwind(((tlFrame*)scopeframe)->caller, res);
}
INTERNAL tlHandle __return(tlArgs* args) {
    return tlTaskPauseResuming(resume_breturn, args);
}

INTERNAL tlHandle resume_bgoto(tlFrame* _frame, tlHandle _res, tlHandle throw) {
    if (throw || !_res) return null;
    tlArgs* args = tlArgsAs(_res);

    int deep = tl_int(tlArgsGet(args, 0));
    trace("RETURN SCOPES: %d", deep);

    // TODO we should return our scoped function, not the first frame ...
    tlHandle res = tlNull;
    if (tlArgsSize(args) == 2) res = tlArgsGet(args, 1);
    if (tlArgsSize(args) > 2) res = tlResultFromArgsSkipOne(args);

    tlBFrame* scopeframe = tlBFrameAs(_frame->caller);
    assert(scopeframe->locals);
    tlBCall* target = tlBEnvGetParentAt(scopeframe->locals, deep)->args;

    tlFrame* frame = (tlFrame*)scopeframe;
    while (frame) {
        trace("%p", frame);
        if (tlBFrameIs(frame)) {
            tlBFrame* bframe = tlBFrameAs(frame);
            trace("found a bframe: %s{.pc=%d,.args=%s) env->args=%s", tl_str(bframe), bframe->pc, tl_str(bframe->args), tl_str(bframe->locals->args));
            // negative pc means a lazy frame, has same locals and bcall as target ... but we ignore it
            if (bframe->pc > 0 && bframe->args == target) {
                scopeframe = bframe;
                break;
            }
            // we keep track of lexically related frames, for when the to return to target is not on stack (captured blocks)
            if (bframe->locals == scopeframe->locals->parent) {
                scopeframe = tlBFrameAs(frame);
                trace("found a scope frame: %s.pc=%d (%d)", tl_str(frame), scopeframe->pc, deep);
            }
        }
        frame = frame->caller;
    }
    assert(scopeframe);
    return tlTaskStackUnwind(((tlFrame*)scopeframe)->caller, res);
}
INTERNAL tlHandle __goto(tlArgs* args) {
    return tlTaskPauseResuming(resume_bgoto, args);
}

INTERNAL void install_bcatch(tlFrame* _frame, tlHandle handler) {
    tlBFrameAs(_frame)->handler = handler;
}
INTERNAL tlHandle _bcatch(tlArgs* args) {
    tlBFrame* frame = tlBFrameAs(tlTaskCurrentFrame(tlTaskCurrent()));
    tlHandle handler = tlArgsBlock(args);
    if (!handler) handler = tlArgsGet(args, 0);
    if (!handler) handler = tlNull;
    frame->handler = handler;
    return tlNull;
}

tlBCall* bcallFromArgs(tlArgs* args) {
    if (tlArgsMapSize(args) == 0) {
        tlBCall* call = tlBCallNew(tlArgsSize(args));
        call->target = tlArgsTarget(args);
        call->fn = tlArgsFn(args);
        for (int i = 0; i < tlArgsSize(args); i++) {
            call->args[i] = tlArgsGet(args, i);
            trace("BCALL[%d] = %s", i, tl_str(call->args[i]));
        }
        return call;
    }

    tlBCall* call = tlBCallNew(tlArgsSize(args) + tlArgsMapSize(args));
    tlList* names = tlListNew(tlArgsSize(args) + tlArgsMapSize(args));
    call->names = names;
    call->target = tlArgsTarget(args);
    call->fn = tlArgsFn(args);
    call->msg = tlArgsMsg(args);
    int i = 0;
    for (; i < tlArgsSize(args); i++) {
        call->args[i] = tlArgsGet(args, i);
        tlListSet_(names, i, tlNull);
        trace("BCALL[%d] = %s", i, tl_str(call->args[i]));
    }

    tlObject* kargs = tlArgsObject(args);
    for (int j = 0; j < tlObjectSize(kargs); j++) {
        call->args[i + j] = tlObjectValueIter(kargs, j);
        tlListSet_(names, i + j, tlStringFromSym(tlObjectKeyIter(kargs, j)));
        trace("BCALL[%d](%s) = %s", i + j, tl_str(tlListGet(names, i + j)), tl_str(call->args[i + j]));
    }
    return call;
}

INTERNAL tlHandle _bclosure_call(tlArgs* args) {
    trace("bclosure.call");
    tlBCall* call = bcallFromArgs(args);
    call->target = null; // TODO unless this was passed in? e.g. method.call(this=target, 10, 10)
    call->fn = tlArgsTarget(args);
    assert(tlBClosureIs(call->fn));
    return beval(tlTaskCurrent(), null, call, 0, null);
}
INTERNAL tlHandle runBClosure(tlHandle _fn, tlArgs* args) {
    trace("bclosure.run");
    tlBCall* call = bcallFromArgs(args);
    call->fn = _fn;
    assert(tlBClosureIs(call->fn));
    return beval(tlTaskCurrent(), null, call, 0, null);
}

INTERNAL tlHandle _blazy_call(tlArgs* args) {
    trace("blazy.call");
    tlBLazy* lazy = tlBLazyAs(tlArgsTarget(args));
    tlBCall* call = bcallFromArgs(args);
    call->target = null;
    call->fn = lazy;
    return beval(tlTaskCurrent(), null, lazy->args, lazy->pc, lazy->locals);
}
INTERNAL tlHandle runBLazy(tlHandle fn, tlArgs* args) {
    trace("blazy.run");
    tlBLazy* lazy = tlBLazyAs(fn);
    tlBCall* call = bcallFromArgs(args);
    call->fn = lazy;
    return beval(tlTaskCurrent(), null, lazy->args, lazy->pc, lazy->locals);
}

INTERNAL tlHandle _blazydata_call(tlArgs* args) {
    trace("blazydata.call");
    return tlBLazyDataAs(tlArgsTarget(args))->data;
}
INTERNAL tlHandle runBLazyData(tlHandle fn, tlArgs* args) {
    trace("blazydata.run");
    return tlBLazyDataAs(fn)->data;
}

static const tlNativeCbs __bcode_natives[] = {
    { "_Module_new", _Module_new },
    { "_module_run", _module_run },
    { "_module_links", _module_links },
    { "_module_link", _module_link },
    { "__list", __list },
    { "__map", __map },
    { "__object", __object },
    { "return", __return },
    { "goto", __goto },
    { 0, 0 }
};

void bcode_init() {
    tl_register_natives(__bcode_natives);
    _tlBClosureKind.klass = tlClassObjectFrom("call", _bclosure_call, null);
    _tlBLazyKind.klass = tlClassObjectFrom("call", _blazy_call, null);
    _tlBLazyDataKind.klass = tlClassObjectFrom("call", _blazydata_call, null);
    _tlBClosureKind.run = runBClosure;
    _tlBLazyKind.run = runBLazy;
    _tlBLazyDataKind.run = runBLazyData;

    INIT_KIND(tlBModuleKind);
    INIT_KIND(tlBDebugInfoKind);
    INIT_KIND(tlBCodeKind);
    INIT_KIND(tlBCallKind);
    INIT_KIND(tlBEnvKind);
    INIT_KIND(tlBClosureKind);
    INIT_KIND(tlBLazyKind);
    INIT_KIND(tlBLazyDataKind);
    INIT_KIND(tlBSendTokenKind);
}

