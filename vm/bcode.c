#include "bcode.h"
#define HAVE_DEBUG 1
#include "trace-on.h"

TL_REF_TYPE(tlBModule);
TL_REF_TYPE(tlBCode);
TL_REF_TYPE(tlBCall);

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

    unsigned int size;
    const uint8_t* code;
};

struct tlBCall {
    tlHead head;
    int size;
    tlHandle target;
    tlHandle fn;
    tlHandle args[];
};

static tlKind _tlBModuleKind;
tlKind* tlBModuleKind = &_tlBModuleKind;

static tlKind _tlBCodeKind;
tlKind* tlBCodeKind = &_tlBCodeKind;

static tlKind _tlBCallKind;
tlKind* tlBCallKind = &_tlBCallKind;

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
void tlBCallAdd(tlBCall* call, tlHandle o) {
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
        switch (op) {
            case OP_END: goto exit;
            case OP_TRUE:  print(" true"); break;
            case OP_FALSE: print(" false"); break;
            case OP_NULL: print(" null"); break;
            case OP_UNDEF: print(" undef"); break;
            case OP_INT: r = dreadsize(&code); print(" int %d", r); break;
            case OP_SYSTEM: o = dreadref(&code, data); print(" system %s", tl_str(o)); break;
            case OP_MODULE: o = dreadref(&code, data); print(" data %s", tl_str(o)); break;
            case OP_GLOBAL: r = dreadsize(&code); print(" linked %s (%s)", tl_str(tlListGet(links, r)), tl_str(tlListGet(linked, r))); break;
            case OP_ENV: r = dreadsize(&code); r2 = dreadsize(&code); print(" env %d %d", r, r2); break;
            case OP_LOCAL: r = dreadsize(&code); print(" local %d", r); break;
            case OP_ARG: r = dreadsize(&code); print(" arg %d", r); break;
            case OP_RESULT: r = dreadsize(&code); print(" result %d", r); break;
            case OP_BIND: o = dreadref(&code, data); print(" data %s", tl_str(o)); break;
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

tlBModule* tlBModuleLink(tlBModule* mod, tlEnv* env) {
    assert(!mod->linked);
    mod->linked = tlListNew(tlListSize(mod->links));
    for (int i = 0; i < tlListSize(mod->links); i++) {
        tlHandle name = tlListGet(mod->links, i);
        tlHandle v = tlEnvGet(env, name);
        print("linking: %s as %s", tl_str(name), tl_str(v));
        tlListSet_(mod->linked, i, v);
    }
    return mod;
}

void beval(tlTask* task, tlBModule* mod) {
    assert(mod->linked);
    assert(mod->body->mod);
    assert(mod == mod->body->mod);

    tlBCode* bcode = mod->body;
    const uint8_t* code = bcode->code;
    tlList* data = bcode->mod->data;
    tlList* links = bcode->mod->links;
    tlList* linked = bcode->mod->linked;

    tlBCall* call = null;
    int r = 0;
    int r2 = 0;
    tlHandle o = null;
    while (true) {
        uint8_t op = *code; code++;
        switch (op) {
            case OP_END: goto exit;
            case OP_TRUE:  fatal(" true"); break;
            case OP_FALSE: fatal(" false"); break;
            case OP_NULL: fatal(" null"); break;
            case OP_UNDEF: fatal(" undef"); break;
            case OP_INT: r = dreadsize(&code); fatal(" int %d", r); break;
            case OP_SYSTEM: o = dreadref(&code, data); fatal(" system %s", tl_str(o)); break;
            case OP_MODULE:
                r = dreadsize(&code);
                o = tlListGet(data, r);
                trace(" %s data %s", tl_str(call), tl_str(o));
                tlBCallAdd(call, o);
            break;
            case OP_GLOBAL:
                r = dreadsize(&code);
                o = tlListGet(linked, r);
                trace(" %s linked %s (%s)", tl_str(call), tl_str(o), tl_str(tlListGet(links, r)));
                tlBCallAdd(call, o);
            break;

            case OP_ENV: r = dreadsize(&code); r2 = dreadsize(&code); fatal(" env %d %d", r, r2); break;
            case OP_LOCAL: r = dreadsize(&code); fatal(" local %d", r); break;
            case OP_ARG: r = dreadsize(&code); fatal(" arg %d", r); break;
            case OP_RESULT: r = dreadsize(&code); fatal(" result %d", r); break;
            case OP_BIND: o = dreadref(&code, data); fatal(" data %s", tl_str(o)); break;
            case OP_STORE: r = dreadsize(&code); fatal(" store %d", r); break;
            case OP_INVOKE:
                o = tlBCallGetFn(call);
                trace(" %s invoke %s", tl_str(call), tl_str(o));
                if (tlNativeIs(o)) {
                    tlArgs* args = tlArgsNew(tlListNew(call->size), null);
                    tlArgsSetTarget_(args, call->target);
                    tlArgsSetFn_(args, call->fn);
                    for (int i = 0; i < call->size; i++) tlArgsSet_(args, i, call->args[i]);
                    tlNativeKind->run(tlNativeAs(o), args);
                    break;
                }
                fatal("don't know how to call: %s", tl_str(o));
            break;
            case OP_FCALL:
                r = dreadsize(&code);
                call = tlBCallNew(r);
                tlBCallAdd(call, tlNull); // no target
                trace(" %s fcall %d", tl_str(call), r);
            break;
            case OP_MCALL: r = dreadsize(&code); fatal(" mcall %d", r); break;
            case OP_BCALL: r = dreadsize(&code); fatal(" bcall %d", r); break;
            case OP_MCALLN: r = dreadsize(&code); fatal(" mcalln %d", r); break;
            case OP_FCALLN: r = dreadsize(&code); fatal(" fcalln %d", r); break;
            case OP_BCALLN: r = dreadsize(&code); fatal(" bcalln %d", r); break;
            default: fatal("OEPS: %x", op);
        }
    }
exit:;
}

