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
        case OP_LOCAL: return "LOCAL";
        case OP_ARG: return "ARG";
        case OP_RESULT: return "RESULT";
        case OP_BIND: return "BIND";
        case OP_STORE: return "STORE";
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
TL_REF_TYPE(tlBCode);
TL_REF_TYPE(tlBCall); // TODO can just be tlArgs
TL_REF_TYPE(tlBEnv);
TL_REF_TYPE(tlBClosure);
TL_REF_TYPE(tlBFrame);
TL_REF_TYPE(tlBLazy);
TL_REF_TYPE(tlBLazyData);

// against better judgement, modules are linked by mutating, or copying if already linked
struct tlBModule {
    tlHead head;
    tlString* name; // name of this module, usually filename
    tlList* data;   // list of constants
    tlList* links;  // names of all global things referenced
    tlList* linked; // when linked, resolved values for all globals
    tlBCode* body;  // starting point; will reference linked and data
};

struct tlBCode {
    tlHead head;
    tlBModule* mod; // back reference to module it belongs

    tlString* name;
    tlList* args;
    tlMap* argnames;
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
    tlHandle fn;
    tlHandle args[];
};

struct tlBEnv {
    tlHead head;
    tlBEnv* parent;
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
    tlHead head;
    tlBCall* args;

    // mutable! but only by addition ...
    tlBEnv* locals;

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
    tlHead ehad;
    tlHandle data;
};

static tlKind _tlBModuleKind = { .name = "BModule" };
tlKind* tlBModuleKind = &_tlBModuleKind;

static tlKind _tlBCodeKind = { .name = "BCode" };
tlKind* tlBCodeKind = &_tlBCodeKind;

static tlKind _tlBCallKind = { .name = "BCall" };
tlKind* tlBCallKind = &_tlBCallKind;

static tlKind _tlBEnvKind = { .name = "BEnv" };
tlKind* tlBEnvKind = &_tlBEnvKind;

static tlKind _tlBClosureKind = { .name = "BClosure" };
tlKind* tlBClosureKind = &_tlBClosureKind;

static tlKind _tlBFrameKind = { .name = "BFrame" };
tlKind* tlBFrameKind = &_tlBFrameKind;

static tlKind _tlBLazyKind = { .name = "BLazy" };
tlKind* tlBLazyKind = &_tlBLazyKind;

static tlKind _tlBLazyDataKind = { .name = "BLazyData" };
tlKind* tlBLazyDataKind = &_tlBLazyDataKind;

tlBModule* tlBModuleNew(tlString* name) {
    tlBModule* mod = tlAlloc(tlBModuleKind, sizeof(tlBModule));
    mod->name = name;
    return mod;
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
tlBClosure* tlBClosureNew(tlBCode* code, tlBEnv* env) {
    tlBClosure* fn = tlAlloc(tlBClosureKind, sizeof(tlBClosure));
    fn->code = code;
    fn->env = env;
    return fn;
}
tlBFrame* tlBFrameNew(tlBCall* args) {
    int calldepth = tlBClosureAs(args->fn)->code->calldepth;
    tlBFrame* frame = tlAlloc(tlBFrameKind, sizeof(tlBFrame) + sizeof(tlHandle) * calldepth);
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
    return lazy;
}

bool tlBCallIsLazy(const tlBCall* call, int arg) {
    arg -= 2; // 0 == target; 1 == fn; 2 == arg[0]
    if (arg < 0 || !call || arg >= call->size || !tlBClosureIs(call->fn)) return false;
    tlHandle entry = tlListGet(tlBClosureAs(call->fn)->code->args, arg);
    return tlTrue == tlListGet(tlListAs(entry), 1);
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
}
tlHandle tlBCallGet(tlBCall* call, int at) {
    assert(at >= 0 && at <= call->size);
    return call->args[at];
}
tlHandle tlBCallGetTarget(tlBCall* call) {
    return call->target;
}
tlHandle tlBCallGetFn(tlBCall* call) {
    return call->fn;
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

static tlHandle decodelit(uint8_t b);
static int dreadsize(const uint8_t** code2) {
    const uint8_t* code = *code2;
    int res = 0;
    while (true) {
        uint8_t b = *code; code++;
        if (b < 0x80) { *code2 = code; return (res << 7) | b; }
        res = res << 6 | (b & 0x3F);
    }
}
static inline int pcreadsize(const uint8_t* ops, int* pc) {
    int res = 0;
    while (true) {
        uint8_t b = ops[(*pc)++];
        if (b < 0x80) { return (res << 7) | b; }
        res = res << 6 | (b & 0x3F);
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
        case 5: return tlMapEmpty();
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

tlBCode* readbytecode(tlBuffer* buf, tlList* data, tlBModule* mod, int size) {
    if (tlBufferSize(buf) < size) fatal("buffer too small");
    int start = tlBufferSize(buf);

    tlBCode* bcode = tlBCodeNew(mod);
    bcode->args = tlListAs(readref(buf, data, null));
    bcode->argnames = tlMapAs(readref(buf, data, null));
    bcode->localnames = tlListAs(readref(buf, data, null));
    bcode->name = tlStringAs(readref(buf, data, null));
    trace(" %s -- %s %s %s", tl_str(bcode->name), tl_str(bcode->args), tl_str(bcode->argnames), tl_str(bcode->localnames));

    // we don't want above data in the bcode ... so skip it
    size -= start - tlBufferSize(buf);
    assert(size > 0);
    uint8_t *code = malloc_atomic(size);
    tlBufferRead(buf, (char*)code, size);
    assert(code[size - 1] == 0);
    bcode->size = size;
    bcode->code = code;

    trace("code: %s", tl_str(bcode));
    return bcode;
}

tlHandle readsizedvalue(tlBuffer* buf, tlList* data, tlBModule* mod, uint8_t b1) {
    int size = readsize(buf);
    assert(size > 0 && size < 1000);
    switch (b1) {
        case 0xE0: // string
            fatal("string %d", size);
            break;
        case 0xE1: // list
            fatal("list %d", size);
            break;
        case 0xE2: // set
            fatal("set %d", size);
            break;
        case 0xE3: // map
            fatal("map %d", size);
            break;
        case 0xE4: // raw
            fatal("raw %d", size);
            break;
        case 0xE5: // bytecode
            trace("bytecode %d", size);
            return readbytecode(buf, data, mod, size);
        case 0xE6: // bignum
            fatal("bignum %d", size);
            break;
        default:
            fatal("unknown value type: %d", b1);
            break;
    }
    fatal("not reached");
    return null;
}

tlHandle readvalue(tlBuffer* buf, tlList* data, tlBModule* mod) {
    if (tlBufferSize(buf) < 1) fatal("buffer too small");
    uint8_t b1 = tlBufferReadByte(buf);
    uint8_t type = b1 & 0xE0;
    uint8_t size = b1 & 0x1F;

    switch (type) {
        case 0x00: { // short string
            trace("short string: %d", size);
            if (tlBufferSize(buf) < size) fatal("buffer too small");
            char *data = malloc_atomic(size + 1);
            tlBufferRead(buf, data, size);
            data[size] = 0;
            return tlStringFromTake(data, size);
        }
        case 0x20: { // short list
            trace("short list: %d", size);
            tlList* list = tlListNew(size);
            for (int i = 0; i < size; i++) {
                tlHandle v = readref(buf, data, null);
                assert(v);
                trace("LIST: %d: %s", i, tl_str(v));
                tlListSet_(list, i, v);
            }
            return list;
        }
        case 0x40: // short set
            fatal("set %d", size);
        case 0x60: { // short map
            trace("short map: %d (%d)", size, tlBufferSize(buf));
            tlList* pairs = tlListNew(size);
            for (int i = 0; i < size; i++) {
                tlHandle key = readref(buf, data, null);
                assert(key);
                tlHandle v = readref(buf, data, null);
                assert(v);
                trace("MAP: %s: %s", tl_str(key), tl_str(v));
                tlListSet_(pairs, i, tlListFrom2(tlSymFromString(key), v));
            }
            return tlMapFromPairs(pairs);
        }
        case 0x80: // short raw
            fatal("raw %d", size);
        case 0xA0: // short bytecode
            trace("short bytecode: %d", size);
            return readbytecode(buf, data, mod, size);
        case 0xC0: { // short size number
            assert(size <= 8); // otherwise it is a float ... not implemented yet
            trace("int %d", size);
            int val = 0;
            for (int i = 0; i < size; i++) {
                val = (val << 8) + tlBufferReadByte(buf);
            }
            return tlINT(val);
        }
        case 0xE0: // sized types
            return readsizedvalue(buf, data, mod, b1);
    }
    fatal("not reached");
    return null;
}

tlHandle deserialize(tlBuffer* buf, tlBModule* mod) {
    trace("parsing: %d", tlBufferSize(buf));

    if (tlBufferReadByte(buf) != 't') return null;
    if (tlBufferReadByte(buf) != 'l') return null;
    if (tlBufferReadByte(buf) != '0') return null;
    if (tlBufferReadByte(buf) != '1') return null;

    int size = 0;
    tlHandle direct = readref(buf, null, &size);
    trace("tl01 %d (%s)", size, tl_str(direct));
    if (direct) { assert(size == 0); return direct; }
    assert(size < 1000); // sanity check

    tlHandle v = null;
    tlList* data = tlListNew(size);
    for (int i = 0; i < size; i++) {
        v = readvalue(buf, data, mod);
        trace("%d: %s", i, tl_str(v));
        assert(v);
        tlListSet_(data, i, v);
    }
    if (mod) {
        mod->data = data;
        mod->links = tlListAs(tlMapGet(v, tlSYM("link")));
        mod->body = tlBCodeAs(tlMapGet(v, tlSYM("body")));
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
    print(" name: %s::%s", tl_str(bcode->mod->name), tl_str(bcode->name));
    print(" <args>");
    for (int i = 0;; i++) {
        tlHandle arg = tlListGet(bcode->args, i);
        if (!arg) break;
        print("   %s, %s, %s", tl_str(tlListGet(arg, 0)), tl_str(tlListGet(arg, 1)), tl_str(tlListGet(arg, 2)));
    }
    print(" </args>");
    print(" <argnames>");
    for (int i = 0;; i++) {
        tlHandle key = tlMapKeyIter(bcode->argnames, i);
        if (!key) break;
        tlHandle value = tlMapValueIter(bcode->argnames, i);
        print("  %s -> %s", tl_str(key), tl_str(value));
    }
    print(" </argnames>");
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
            case OP_MCALLN: case OP_FCALLN: case OP_BCALLN:
                r = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d", opc, op, op_name(op), r);
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
            case OP_ENV:
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d %d", opc, op, op_name(op), r, r2);
                break;
            case OP_RESULT:
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d(%s) <- %d", opc, op, op_name(op),
                        r2, tl_str(tlListGet(bcode->localnames, r2)), r);
                break;
            case OP_BIND:
                o = pcreadref(ops, &pc, data);
                print(" % 3d 0x%X %s: %s", opc, op, op_name(op), tl_str(o));
                if (tlBCodeIs(o)) disasm(tlBCodeAs(o));
                break;
            case OP_STORE:
                r = pcreadsize(ops, &pc);
                print(" % 3d 0x%X %s: %d(%s) <-", opc, op, op_name(op), r, tl_str(tlListGet(bcode->localnames, r)));
                break;
            default: print("OEPS: %d 0x%X %s", opc, op, op_name(op));
        }
    }
exit:;
    print("</code>");
}

void pprint(tlHandle v) {
    if (tlBCodeIs(v)) {
        tlBCode* code = tlBCodeAs(v);
        disasm(code);
        return;
    }
    if (tlMapIs(v)) {
        tlMap* map = tlMapAs(v);
        print("{");
        for (int i = 0;; i++) {
            tlHandle v = tlMapValueIter(map, i);
            if (!v) break;
            tlHandle k = tlMapKeyIter(map, i);
            print("%s:", tl_str(k));
            pprint(v);
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
            pprint(v);
        }
        print("]");
        return;
    }
    print("%s", tl_str(v));
}

// verify and fill in call depth and balancing, and use of locals
void tlBCodeVerify(tlBCode* bcode) {
    int locals = 0;
    int maxdepth = 0;
    int depth = 0;

    assert(bcode->mod);
    const uint8_t* code = bcode->code;
    tlList* data = bcode->mod->data;

    tlHandle v;
    int parent;
    int at;

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
            case OP_MODULE: dreadsize(&code); break;
            case OP_GLOBAL: dreadsize(&code); break;
            case OP_ENV:
                parent = dreadsize(&code);
                at = dreadsize(&code);
                assert(parent >= 0 && parent <= 200);
                assert(at >= 0); // todo and check parent locals
                break;
            case OP_LOCAL:
                at = dreadsize(&code);
                assert(at <= locals);
                break;
            case OP_ARG: dreadsize(&code); break;
            case OP_RESULT: dreadsize(&code); dreadsize(&code); break;
            case OP_BIND:
                v = dreadref(&code, data);
                tlBCodeVerify(tlBCodeAs(v)); // TODO pass in our locals somehow for ENV
                break;
            case OP_STORE:
                at = dreadsize(&code);
                assert(locals == at);
                locals++;
                break;

            case OP_INVOKE:
                assert(depth > 0);
                if (depth > maxdepth) maxdepth = depth;
                depth--;
                break;
            case OP_MCALL:
            case OP_FCALL:
            case OP_BCALL:
            case OP_MCALLN:
            case OP_FCALLN:
            case OP_BCALLN:
                dreadsize(&code);
                depth++;
                break;
            default: print("OEPS: %x", op);
        }
    }
exit:;
     assert(depth == 0);
     print("verified; locals: %d, calldepth: %d, %s", locals, maxdepth, tl_str(bcode));
     bcode->locals = locals;
     bcode->calldepth = maxdepth;
}

tlBModule* tlBModuleLink(tlBModule* mod, tlEnv* env) {
    assert(!mod->linked);
    mod->linked = tlListNew(tlListSize(mod->links));
    for (int i = 0; i < tlListSize(mod->links); i++) {
        tlHandle name = tlListGet(mod->links, i);
        tlHandle v = tlEnvGet(env, tlSymFromString(name));
        print("linking: %s as %s", tl_str(name), tl_str(v));
        tlListSet_(mod->linked, i, v);
    }
    tlBCodeVerify(mod->body);
    return mod;
}

tlHandle beval(tlTask* task, tlBFrame* frame, tlBCall* args, int lazypc, tlBEnv* locals);

tlHandle tlInvoke(tlTask* task, tlBCall* call) {
    tlHandle fn = tlBCallGetFn(call);
    trace(" %s invoke %s", tl_str(call), tl_str(fn));
    if (tlNativeIs(fn)) {
        tlArgs* args = tlArgsNew(tlListNew(call->size), null);
        tlArgsSetTarget_(args, call->target);
        tlArgsSetFn_(args, call->fn);
        for (int i = 0; i < call->size; i++) tlArgsSet_(args, i, call->args[i]);
        return tlNativeKind->run(tlNativeAs(fn), args);
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
    fatal("don't know how to invoke: %s", tl_str(fn));
    return null;
}

static tlBLazy* create_lazy(const uint8_t* ops, int* ppc, tlBCall* args, tlBEnv* locals) {
    // match CALLs with INVOKEs and wrap it for later execution
    // notice bytecode vs literals are such that OP_CALL cannot appear in literals
    int pc = *ppc;
    int start = pc - 2; // we re-interpret OP_CALL and SIZE when working on lazy; TODO - 2 is not always correct?
    int depth = 1;
    while (true) {
        uint8_t op = ops[pc++];
        if (op == OP_INVOKE) {
            depth--;
            if (!depth) {
                *ppc = pc;
                trace("%d", start);
                return tlBLazyNew(args, locals, start);
            }
        } else if (op & 0x20) { // OP_CALL mask
            depth++;
        }
    }
}

// TODO this is not really needed, compiler or verifier can tell if a real locals needs to be realized
static void ensure_locals(tlHandle (**data)[], tlBEnv** locals, tlBClosure* fn) {
    if (*locals) {
        assert(&(*locals)->data == *data);
        return;
    }
    *locals = tlBEnvNew(fn->code->localnames, fn->env);
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
    ensure_locals(data, locals, tlBClosureAs(args->fn));
    *frame = tlBFrameNew(args);
    (*frame)->locals = *locals;
    int calldepth = tlBClosureAs(args->fn)->code->calldepth;
    for (int i = 0; i < calldepth; i++) {
        (*frame)->calls[i] = (**calls)[i];
    }
    *calls = &(*frame)->calls; // switch backing store
}

tlHandle beval(tlTask* task, tlBFrame* frame, tlBCall* args, int lazypc, tlBEnv* lazylocals) {
    assert(task);
    assert(frame || args);
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

    int calltop = -1; // current call index in the call stack
    tlBCall* call = null; // current call, if any
    int arg = 0; // current argument to a call
    bool lazy = false; // if current call[arg] is lazy

    uint8_t op = 0; // current opcode
    tlHandle v = null; // tmp result register

    // when we get here, it is because of the following reasons:
    // 1. we start a closure
    // 2. we start a lazy call
    // 3. we did an invoke, and paused the task, but now the result is ready; see OP_INVOKE
    // 4. we set a method name to a call, and resolving it paused the task; see end of this function
    if (frame) { // 3 or 4
        assert(!args);
        args = frame->args;
        pc = frame->pc;
        calls = &frame->calls;
        lazylocals = frame->locals;
        locals = &lazylocals->data;
        v = tlNull; // TODO tlTaskValue(task);

        // find current call, we mark non current call by setting sign bit
        assert(bcode->calldepth > 0); // cannot be zero, or we would never suspend
        for (calltop = 0; (*calls)[calltop].at < 0; calltop++);
        call = (*calls)[calltop].call;
        arg = (*calls)[calltop].at;
        assert(calltop >= 0 && calltop < bcode->calldepth);
        assert(call || calltop == 0);

        trace("resume eval; %s %s; %s locals: %d call: %d/%d", tl_str(frame), tl_str(v),
                tl_str(bcode), bcode->locals, calltop, bcode->calldepth);
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
    }

again:;
    op = ops[pc++];
    if (!op) return v; // OP_END
    assert(op & 0xC0);

    lazy = tlBCallIsLazy(call, arg);
    trace("LAZY? %d[%d] %s", calltop, arg - 2, lazy?"yes":"no");

    // is it a call
    if (op & 0x20) {
        int size = pcreadsize(ops, &pc);
        trace("%d call op: 0x%X %s - %d", pc, op, op_name(op), size);

        if (lazy) {
            trace("lazy call");
            ensure_locals(&locals, &lazylocals, closure);
            v = create_lazy(ops, &pc, args, lazylocals);
            tlBCallAdd_(call, v, arg++);
            goto again;
        }

        // create a new call
        if (calltop >= 0) (*calls)[calltop].at = -arg; // store current arg, marked as not current
        arg = 0;
        call = tlBCallNew(size);
        calltop++;
        assert(calltop >= 0 && calltop < bcode->calldepth);
        (*calls)[calltop] = (CallEntry){ .at = 0, .call = call };

        // for non methods, skip target
        if (op & 0x0F) tlBCallAdd_(call, tlNull, arg++);
        // load names if it is a named call
        if (op & 0x10) {
            int at = pcreadsize(ops, &pc);
            tlBCallSetNames_(call, tlListAs(tlListGet(data, at)));
        }
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
            calltop--;
            if (calltop >= 0) {
                call = (*calls)[calltop].call;
                arg = (*calls)[calltop].at *= -1; // mark as current again
            } else {
                call = null;
                arg = 0;
            }
            trace("invoke: %s", tl_str(invoke));
            v = tlInvoke(task, invoke);
            if (!v) { // task paused instead of actually doing work, suspend and return
                ensure_frame(&calls, &locals, &frame, &lazylocals, args);
                frame->pc = pc;
                fatal("almost :)");
                return v;
            }
            break;
        }
        case OP_SYSTEM:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
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
        case OP_ENV:
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlBEnv* parent = tlBEnvGetParentAt(closure->env, depth);
            v = tlBEnvGet(parent, at);
            trace("env[%d][%d] -> %s", depth, at, tl_str(v));
            break;
        case OP_ARG:
            at = pcreadsize(ops, &pc);
            v = tlBCallGet(args, at);
            trace("args[%d] %s", at, tl_str(v));
            break;
        case OP_LOCAL:
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            v = (*locals)[at];
            trace("local %d -> %s", at, tl_str(v));
            break;
        case OP_BIND:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            ensure_locals(&locals, &lazylocals, closure);
            v = tlBClosureNew(tlBCodeAs(v), lazylocals);
            trace("%d bind %s", pc, tl_str(v));
            break;
        case OP_RESULT:
            at = pcreadsize(ops, &pc);
            v = tlResultGet(v, at);
            at = pcreadsize(ops, &pc);
            trace("result %d <- %s", at, tl_str(v));
            break;
        case OP_STORE:
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            (*locals)[at] = v;
            trace("store %d <- %s", at, tl_str(v));
            break;
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
    tlBCallAdd_(call, v, arg++);

    // TODO resolve method at object ...
    if (arg == 2 && call->target && tlStringIs(call->fn)) {
        fatal("oeps ... hehe methods");
    }
    goto again;
}

tlHandle beval_module(tlTask* task, tlBModule* mod) {
    tlBClosure* fn = tlBClosureNew(mod->body, null);
    tlBCall* call = tlBCallNew(0);
    call->fn = fn;
    return beval(task, null, call, 0, null);
}

