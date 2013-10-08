#include "bcode.h"
#define HAVE_DEBUG 1
#include "trace-on.h"

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
TL_REF_TYPE(tlBClosure);
TL_REF_TYPE(tlBFrame);

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

    uint32_t locals;
    uint32_t calldepth;
    uint32_t size;
    const uint8_t* code;
};

struct tlBCall {
    tlHead head;
    bool lazy;
    int size;
    tlHandle target;
    tlHandle fn;
    tlHandle args[];
};

struct tlBClosure {
    tlHead head;
    tlBCode* code;
    tlEnv* env;
};

struct tlBFrame {
    tlHead head;
    tlBClosure* fn;
    tlEnv* locals;

    // save/restore
    uint32_t pc;
    uint32_t call;
    tlBCall** calls;
};

static tlKind _tlBModuleKind;
tlKind* tlBModuleKind = &_tlBModuleKind;

static tlKind _tlBCodeKind;
tlKind* tlBCodeKind = &_tlBCodeKind;

static tlKind _tlBCallKind;
tlKind* tlBCallKind = &_tlBCallKind;

static tlKind _tlBClosureKind;
tlKind* tlBClosureKind = &_tlBClosureKind;

static tlKind _tlBFrameKind;
tlKind* tlBFrameKind = &_tlBFrameKind;

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
tlBClosure* tlBClosureNew(tlBCode* code, tlEnv* env) {
    tlBClosure* fn = tlAlloc(tlBClosureKind, sizeof(tlBClosure));
    fn->code = code;
    fn->env = env;
    return fn;
}
tlBFrame* tlBFrameNew(tlBClosure* fn) {
    tlBFrame* frame = tlAlloc(tlBFrameKind, sizeof(tlBFrame));
    frame->fn = fn;
    return frame;
}
void tlBCallAdd_(tlBCall* call, tlHandle o) {
    trace("call add: %s %s %s", tl_str(call->target), tl_str(call->fn), tl_str(call->args[0]));
    if (!call->target) { call->target = o; return; }
    if (!call->fn) { call->fn = o; return; }
    for (int i = 0;; i++) {
        assert(i < 128);
        if (call->args[i]) continue;
        call->args[i] = o;
        break;
    }
}
void tlBCallSetNames_(tlBCall* call, tlList* names) {
}
tlHandle* tlBCallGet(tlBCall* call, int at) {
    assert(at >= 0 && at <= call->size);
    return call->args[at];
}
tlHandle* tlBCallGetTarget(tlBCall* call) {
    return call->target;
}
tlHandle* tlBCallGetFn(tlBCall* call) {
    return call->fn;
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
static int pcreadsize(const uint8_t* ops, int* pc) {
    int res = 0;
    while (true) {
        uint8_t b = ops[*pc++];
        if (b < 0x80) { return (res << 7) | b; }
        res = res << 6 | (b & 0x3F);
    }
}
static tlHandle dreadref(const uint8_t** code, tlList* data) {
    uint8_t b = **code;
    if (b > 0xC0) {
        *code = *code + 1;
        return decodelit(b);
    }
    int r = dreadsize(code);
    if (data) return tlListGet(data, r);
    return null;
}

// 11.. ....
static tlHandle decodelit(uint8_t b1) {
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

tlHandle readsizedvalue(tlBuffer* buf, tlList* data, uint8_t b1) {
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
            fatal("bytecode %d", size);
            break;
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
        case 0xA0: { // short bytecode
            trace("short bytecode: %d", size);
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
            return readsizedvalue(buf, data, b1);
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

static void disasm(tlBCode* bcode) {
    assert(bcode->mod);
    const uint8_t* code = bcode->code;
    tlList* data = bcode->mod->data;
    tlList* links = bcode->mod->links;
    tlList* linked = bcode->mod->linked;

    print("<code>");
    int r = 0; int r2 = 0; tlHandle o = null;
    while (true) {
        uint8_t op = *code; code++;
        print("op: 0x%X %s", op, op_name(op));
        switch (op) {
            case OP_END: goto exit;
            case OP_TRUE:  print(" true"); break;
            case OP_FALSE: print(" false"); break;
            case OP_NULL: print(" null"); break;
            case OP_UNDEF: print(" undef"); break;
            case OP_INT: r = dreadsize(&code); print(" int %d", r); break;
            case OP_SYSTEM: o = dreadref(&code, data); print(" system %s", tl_str(o)); break;
            case OP_MODULE:
                o = dreadref(&code, data); print(" data %s", tl_str(o));
                if (tlBCodeIs(o)) disasm(tlBCodeAs(o));
                break;
            case OP_GLOBAL: r = dreadsize(&code); print(" linked %s (%s)", tl_str(tlListGet(links, r)), tl_str(tlListGet(linked, r))); break;
            case OP_ENV: r = dreadsize(&code); r2 = dreadsize(&code); print(" env %d %d", r, r2); break;
            case OP_LOCAL: r = dreadsize(&code); print(" local %d", r); break;
            case OP_ARG: r = dreadsize(&code); print(" arg %d", r); break;
            case OP_RESULT: r = dreadsize(&code); print(" result %d", r); break;
            case OP_BIND:
                o = dreadref(&code, data); print(" bind %s", tl_str(o));
                if (tlBCodeIs(o)) disasm(tlBCodeAs(o));
                break;
            case OP_STORE: r = dreadsize(&code); print(" store %d", r); break;
            case OP_INVOKE: print(" invoke"); break;
            case OP_MCALL: r = dreadsize(&code); print(" mcall %d", r); break;
            case OP_FCALL: r = dreadsize(&code); print(" fcall %d", r); break;
            case OP_BCALL: r = dreadsize(&code); print(" bcall %d", r); break;
            case OP_MCALLN: r = dreadsize(&code); print(" mcalln %d", r); break;
            case OP_FCALLN: r = dreadsize(&code); print(" fcalln %d", r); break;
            case OP_BCALLN: r = dreadsize(&code); print(" bcalln %d", r); break;
            default: print("OEPS: %x", op);
        }
    }
exit:;
    print("</code>");
}

void pprint(tlHandle v) {
    if (tlBCodeIs(v)) {
        tlBCode* code = tlBCodeAs(v);
        print("BYTECODE: %s::%s", tl_str(code->mod->name), tl_str(code->name));
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
            case OP_RESULT: dreadsize(&code); break;
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
                print("DEPTH--: %d", depth);
                break;
            case OP_MCALL:
            case OP_FCALL:
            case OP_BCALL:
            case OP_MCALLN:
            case OP_FCALLN:
            case OP_BCALLN:
                dreadsize(&code);
                depth++;
                print("DEPTH++: %d", depth);
                break;
            default: print("OEPS: %x", op);
        }
    }
exit:;
     assert(depth == 0);
     print("verified; locals: %d, calldepth: %d, %s", locals, maxdepth, tl_str(bcode));
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

tlHandle tlInvoke(tlBCall* call) {
    tlHandle fn = tlBCallGetFn(call);
    trace(" %s invoke %s", tl_str(call), tl_str(fn));
    if (tlNativeIs(fn)) {
        tlArgs* args = tlArgsNew(tlListNew(call->size), null);
        tlArgsSetTarget_(args, call->target);
        tlArgsSetFn_(args, call->fn);
        for (int i = 0; i < call->size; i++) tlArgsSet_(args, i, call->args[i]);
        return tlNativeKind->run(tlNativeAs(fn), args);
    }
    fatal("don't know how to invoke: %s", tl_str(fn));
    return null;
}

tlHandle create_lazy(const uint8_t* ops, int* ppc, tlBFrame* frame) {
    // match CALLs with INVOKEs and wrap it for later execution
    // notice bytecode vs literals are such that OP_CALL cannot appear in literals
    int pc = *ppc;
    int start = pc - 2; // we re-interpret OP_CALL and SIZE when working on lazy
    int depth = 1;
    while (true) {
        uint8_t op = ops[pc++];
        if (op == OP_INVOKE) {
            depth--;
            if (!depth) {
                *ppc = pc;
                print("%d", start);
                return tlNull;// tlBLazyCallNew(args, locals, start);
            }
        } else if (op & 0x20) { // OP_CALL mask
            depth++;
        }
    }
}

tlHandle beval(tlTask* task, tlBFrame* frame, tlBCall* args) {
    assert(task);
    assert(frame || args);
    tlBClosure* closure = tlBClosureAs(args->fn);
    tlBCode* bcode = closure->code;
    tlBModule* mod = bcode->mod;
    tlList* data = mod->data;
    const uint8_t* ops = bcode->code;

    // call stack and program counter; saved and restored into frame
    int pc = 0;
    int call = -1;
    tlBCall* calls[bcode->calldepth];
    tlHandle locals[bcode->locals];

    bool lazy = false; // if current call is lazy
    uint8_t op = 0; // current opcode
    tlHandle v = null; // tmp result register

    // when we get here, it is because of the following reasons:
    // 1. we start a closure
    // 2. we start a lazy call
    // 3. we did an invoke, and paused the task, but now the result is ready
    // 4. we set a method name to a call, and resolving it paused the task
    if (frame) {
        pc = frame->pc;
        call = frame->call;
        //calls = frame->calls;
        v = tlNull; // task->value;
        goto resume; // for speed and code density
    }

again:;
    op = ops[pc++];
    if (!op) return tlNull; //task->value; // OP_END
    trace("op: 0x%X %s", op, op_name(op));
    //assert(op & 0xC0);

    lazy = call >= 0 && calls[call]->lazy;

    // is it a call
    if (op & 0x20) {
        int size = pcreadsize(ops, &pc);
        trace("call op: %s - %d", op_name(op), size);

        if (lazy) {
            trace("lazy call");
            create_lazy(ops, &pc, frame);
            goto again;
        }

        assert(call >= -1 && call < bcode->calldepth);
        call++;
        calls[call] = tlBCallNew(size);
        // for non methods, skip target
        if (op & 0x0F) tlBCallAdd_(calls[call], null);
        // load names if it is a named call
        if (op & 0x10) {
            int at = pcreadsize(ops, &pc);
            tlBCallSetNames_(calls[call], tlListAs(tlListGet(data, at)));
        }
        goto again;
    }

    trace("data op: %s", op_name(op));
    int at;
    int depth;

    switch (op) {
        case OP_INVOKE: {
            assert(call >= 0);
            assert(!lazy);
            trace("invoke: %s", tl_str(calls[call]));
            v = tlInvoke(calls[call]);
            if (!v) return v; // task paused instead of actually doing work
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
            tlBCallAdd_(calls[call], v);
        break;
        case OP_GLOBAL:
            at = pcreadsize(ops, &pc);
            v = tlListGet(mod->linked, at);
            trace("linked %s (%s)", tl_str(v), tl_str(tlListGet(mod->links, at)));
            tlBCallAdd_(calls[call], v);
        break;
        case OP_ENV:
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            fatal("env; depth: %d at: %d", depth, at);
        case OP_ARG:
            at = pcreadsize(ops, &pc);
            v = tlBCallGet(args, at);
            trace("arg %s", tl_str(v));
            break;
        case OP_LOCAL:
            at = pcreadsize(ops, &pc);
            v = locals[at];
            trace("local %d -> %s", at, tl_str(v));
            break;
        case OP_BIND:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            v = tlBClosureNew(tlBCodeAs(v), null);
            trace("bind %s", tl_str(v));
            break;
        case OP_RESULT:
            at = pcreadsize(ops, &pc);
            v = tlResultGet(v, at);
            at = pcreadsize(ops, &pc);
            trace("result %d <- %s", at, tl_str(v));
        case OP_STORE:
            at = pcreadsize(ops, &pc);
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

    // if not in a call, store results in the task
    if (call == -1) {
        //tlTaskSetValue(task, v);
        //if (lazyeval) return v;
        goto again;
    }

    // if setting a lazy argument, wrap the data
    if (lazy) {
        fatal("oeps .. hehe lazy");
        //v = tlLazyDataNew(v);
    }

    // set the data to the call
    assert(call >= 0);
    tlBCallAdd_(calls[call], v);

    // resolve method at object ...
    if (calls[call]->target && tlSymIs(calls[call]->fn)) {
        fatal("oeps ... hehe methods");
    }

    goto again;
}

tlHandle beval_module(tlTask* task, tlBModule* mod) {
    tlBClosure* fn = tlBClosureNew(mod->body, null);
    tlBCall* call = tlBCallNew(0);
    call->fn = fn;
    return beval(task, null, call);
}

