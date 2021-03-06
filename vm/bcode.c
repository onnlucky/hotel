// author: Onne Gorter, license: MIT (see license.txt)

// the bytecode interpreter, is in a bad need to be split up, but this is where most bytecode handling happens

#include "platform.h"
#include "bcode.h"

#include "value.h"
#include "task.h"
#include "args.h"
#include "frame.h"
#include "sym.h"
#include "list.h"
#include "object.h"
#include "task.h"
#include "lock.h"
#include "eval.h"
#include "vm.h"
#include "map.h"

#include "../llib/lhashmap.h"

static tlNative* g_goto_native;
static tlNative* g_identity;
static void unwindForGoto(tlTask* task, int deep, tlHandle value);
static tlHandle resumeBCall(tlTask* task, tlFrame* frame, tlHandle res, tlHandle throw);

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
        case OP_ENVARG: return "ENVARG";
        case OP_ENV: return "ENV";
        case OP_ARG: return "ARG";
        case OP_LOCAL: return "LOCAL";
        case OP_ARGS: return "ARGS";
        case OP_ENVARGS: return "ENVARGS";
        case OP_THIS: return "THIS";
        case OP_ENVTHIS: return "ENVTHIS";
        case OP_BIND: return "BIND";
        case OP_STORE: return "STORE";
        case OP_RSTORE: return "RESULT";
        case OP_INVOKE: return "INVOKE";
        case OP_CERR: return "CERR";
        case OP_VGET: return "VGET";
        case OP_VSTORE: return "VSTORE";
        case OP_VRSTORE: return "VRSTORE";
        case OP_EVGET: return "EVGET";
        case OP_EVSTORE: return "EVSTORE";
        case OP_EVRSTORE: return "EVRSTORE";
        case OP_MCALL: return "MCALL";
        case OP_FCALL: return "FCALL";
        case OP_BCALL: return "BCALL";
        case OP_CCALL: return "CCALL";
        case OP_SCALL: return "SCALL";
        case OP_MCALLN: return "MCALLN";
        case OP_FCALLN: return "FCALLN";
        case OP_BCALLN: return "BCALLN";
        case OP_MCALLS: return "MCALLS";
        case OP_MCALLNS: return "MCALLNS";
        default: return "<error>";
    }
}

static tlHandle resumeBFrame(tlTask* task, tlFrame* _frame, tlHandle res, tlHandle throw);
tlDebugger* tlDebuggerFor(tlTask* task);
bool tlDebuggerStep(tlDebugger* debugger, tlTask* task, tlCodeFrame* frame);

enum { kCodeHasLazy = 1 };

// against better judgement, modules are linked by mutating
struct tlBModule {
    tlHead head;
    tlString* name; // name of this module, usually filename
    tlList* data;   // list of constants
    tlList* links;  // names of all global things referenced
    tlBCode* body;  // starting point; will reference linked and data
    tlList* linked; // when linked, resolved values for all globals
    // TODO add tlMap* globals as the visible env at load time
};

struct tlBLazy {
    tlHead head;
    tlArgs* args;
    tlEnv* locals;
    int pc; // where the actuall call is located in the code
};
struct tlBLazyData {
    tlHead head;
    tlHandle data;
};
struct tlBSendToken {
    tlHead head;
    tlSym method;
    bool safe;
};

static tlKind _tlBModuleKind = { .name = "BModule" };
tlKind* tlBModuleKind;

static tlKind _tlBDebugInfoKind = { .name = "BDebugInfo" };
tlKind* tlBDebugInfoKind;

static tlKind _tlBCodeKind = { .name = "BCode" };
tlKind* tlBCodeKind;

static tlKind _tlBClosureKind = { .name = "Function" };
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
tlBModule* tlBModuleCopy(tlBModule* mod) {
    fatal("does not work as advertised: body is fixed to the old module");
    tlBModule* nmod = tlAlloc(tlBModuleKind, sizeof(tlBModule));
    nmod->name = mod->name;
    nmod->data = mod->data;
    nmod->links = mod->links;
    nmod->linked = mod->linked;
    nmod->body = mod->body;
    return nmod;
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
tlString* tlBCodeName(tlBCode* code) {
    if (code->debuginfo) return code->debuginfo->name;
    return null;
}
tlBClosure* tlBClosureNew(tlBCode* code, tlEnv* env) {
    tlBClosure* fn = tlAlloc(tlBClosureKind, sizeof(tlBClosure));
    fn->code = code;
    fn->env = env;
    return fn;
}

bool tlCodeFrameIs(tlHandle v) {
    return tlFrameIs(v) && (tlFrameAs(v)->resumecb == resumeBFrame);
}
tlCodeFrame* tlCodeFrameAs(tlHandle v) {
    assert(tlCodeFrameIs(v));
    return (tlCodeFrame*)v;
}
tlCodeFrame* tlCodeFrameCast(tlHandle v) {
    if (!tlCodeFrameIs(v)) return null;
    return (tlCodeFrame*)v;
}

tlCodeFrame* tlCodeFrameNew(tlArgs* args, tlEnv* locals) {
    tlBClosure* closure = tlBClosureAs(args->fn);
    tlBCode* code = closure->code;
    tlCodeFrame* frame = tlFrameAlloc(resumeBFrame, sizeof(tlCodeFrame) + sizeof(CallEntry) * code->calldepth);
    if (!locals) {
        frame->locals = tlEnvNew(code->localnames, closure->env);
        frame->locals->args = args;
    } else {
        frame->locals = locals;
    }
    return frame;
}

tlEnv* tlCodeFrameEnv(tlFrame* frame) {
    return tlCodeFrameAs(frame)->locals;
}

tlBLazy* tlBLazyNew(tlArgs* args, tlEnv* locals, int pc) {
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

tlBSendToken* tlBSendTokenNew(tlSym method, bool safe) {
    tlBSendToken* token = tlAlloc(tlBSendTokenKind, sizeof(tlBSendToken));
    token->method = method;
    token->safe = safe;
    return token;
}

// TODO instead of argspec as list, do this "raw"
// TODO speed this up by using an index of sorts
static int findNamedArg(tlBCode* code, tlSym name) {
    if (!name) return -1;
    int size = tlListSize(code->argspec);
    for (int i = 0; i < size; i++) {
        tlHandle spec = tlListGet(code->argspec, i);
        assert(spec);
        if (spec == tlNull) continue;
        if (name == tlListGet(tlListAs(spec), 0)) return i; // 0=name 1=default 2=lazy
    }
    return -1;
}

static bool isNameInCall(tlBCode* code, tlList* names, int arg) {
    tlHandle spec = tlListGet(code->argspec, arg);
    if (!spec || spec == tlNull) return false;
    tlHandle name = tlListGet(tlListAs(spec), 0);
    if (!name || name == tlNull) return false;
    int size = tlListSize(names);
    for (int i = 0; i < size; i++) {
        if (name == tlListGet(names, i)) return true;
    }
    return false;
}

// TODO this is dog slow ...
static bool tlBCallIsLazy(tlArgs* call, int arg) {
    arg -= 2; // 0 == target; 1 == fn; 2 == arg[0]
    if (arg < 0 || !call || arg >= tlArgsRawSizeInline(call)) return false;
    if (!tlBClosureIs(call->fn)) return false;

    tlBCode* code = tlBClosureAs(call->fn)->code;
    if (!tlflag_isset(code, kCodeHasLazy)) return false;

    tlList* names = tlArgsNamesInline(call);
    if (names) {
        tlHandle name = tlListGet(names, arg);
        if (tlSymIs(name)) {
            arg = findNamedArg(code, name);
            trace("named arg is numer: %d", arg);
            if (arg == -1) return false;
        } else {
            // first figure out the which unnamed argument we are
            int unnamed = 0;
            for (int a = 0; a < arg; a++) {
                if (!tlSymIs(tlListGet(names, a))) unnamed++;
            }
            trace("unnamed arg: %d", unnamed);
            // then figure out which arg that maps to
            for (arg = 0;; arg++) {
                if (!isNameInCall(code, names, arg)) {
                    if (unnamed == 0) break;
                    unnamed -= 1;
                }
            }
            trace("unnamed arg is numer: %d", arg);
        }
    }

    tlHandle spec = tlListGet(tlBClosureAs(call->fn)->code->argspec, arg);
    if (!spec || spec == tlNull) return false;
    return tlTrue == tlListGet(tlListAs(spec), 2); // 0=name 1=default 2=lazy
}

// TODO we should actually make the bytecode use an exact "builder" pattern against call->data layout
static void tlBCallAdd_(tlArgs* call, tlHandle o, int arg) {
    assert(arg >= 0);
    if (arg == 0) {
        trace("%s.target <- %s", tl_str(call), tl_str(o));
        if (o) tlArgsSetTarget_(call, o);
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
    tlArgsSet_(call, arg, o);
}

static int tlBCallNameIndex(tlList* names, tlSym name) {
    int size = tlListSize(names);
    for (int i = 0; i < size; i++) {
        if (name == tlListGet(names, i)) return i;
    }
    return -1;
}

// TODO this is way to expensive ...
// get args[n] where n represents n'th original argument, incase names have to be matched
tlHandle tlBCallGetExtra(tlArgs* call, int at, tlBCode* code) {
    tlList* argspec = tlListAs(tlListGet(code->argspec, at));
    tlSym name = tlSymCast(tlListGet(argspec, 0));
    trace("ARG(%d)=%s", at, tl_str(name));

    if (name == s_this) return tlOR_UNDEF(tlArgsTargetInline(call));

    tlList* names = tlArgsNames(call);
    if (names) {
        // figure out if this argument was explicitly passed in as named param
        tlHandle v = tlArgsGetNamed(call, name);
        if (v) {
            trace("ARG(%d) name=%s value=%s", at, tl_str(name), tl_repr(v));
            return v;
        }

        // this param was not named, figure out how many other unnamed arguments preceded us
        int skipped = 0;
        for (int i = 0; i < at; i++) {
            tlHandle prevname = tlListGet(tlListGet(code->argspec, i), 0);
            if (tlBCallNameIndex(names, prevname) < 0) skipped++;
        }
        at = skipped;
    }

    tlHandle v = tlArgsGet(call, at);
    trace("ARG(%d) value=%s", at, tl_repr(v));
    if (v) return v;

    v = tlListGet(argspec, 1);
    bool lazy = tl_bool(tlListGet(argspec, 2));
    trace("ARG(%d) default=%s%s", at, tl_repr(v), lazy?" (lazy)":"");
    if (lazy) return tlBLazyDataNew(v);
    if (!v) return tlNull;
    return v;
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
        default: return tlINT(lit - 8);
    }
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

tlSym readString(tlBuffer* buf, int size, const char** error) {
    trace("string: %d", size);
    if (tlBufferSize(buf) < size) FAIL("not enough data");
    char *data = malloc_atomic(size + 1);
    tlBufferRead(buf, data, size);
    data[size] = 0;
    return tlSymFromTake(data, size);
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

    // inline [null, "block"] to speed up if statements
    if (tlListEquals(TL_NAMED_NULL_BLOCK, list)) {
        return TL_NAMED_NULL_BLOCK;
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

    // TODO would be much better as 3 lists, names, defauls, lazy
    tlHandle argspec = readref(buf, data, null);
    if (!tlListIs(argspec)) FAIL("code[2] must be the argspec (list)");
    bcode->argspec = tlListAs(argspec);

    // cache hasLazy
    for (int i = 0, l = tlListSize(bcode->argspec); i < l; i++) {
        tlList* spec = tlListAs(tlListGet(bcode->argspec, i));
        if (!tl_bool(tlListGet(spec, 2))) continue;
        tlflag_set(bcode, kCodeHasLazy);
    }

    tlHandle localnames = readref(buf, data, null);
    if (!tlListIs(localnames)) FAIL("code[3] must be the localnames (list)");
    bcode->localnames = tlListAs(localnames);
    trace(" %s -- %s %s", tl_str(tlBCodeName(bcode)), tl_str(bcode->argspec), tl_str(bcode->localnames));

    // TODO optional, but only for now
    tlHandle localvars = readref(buf, data, null);
    bcode->localvars = tlListCast(localvars);

    tlHandle debuginfo;
    if (tlObjectIs(localvars)) {
        debuginfo = localvars;
    } else {
        debuginfo = readref(buf, data, null);
    }
    if (!tlObjectIs(debuginfo) || tlNullIs(debuginfo)) FAIL("code[4] must be debuginfo (object|null)");

    tlBDebugInfo* info = tlBDebugInfoNew();
    info->name = tlStringAs(name);
    if (tlObjectIs(debuginfo)) {
        trace("debuginfo: %s", tl_repr(debuginfo));
        info->line = tlIntCast(tlObjectGet(debuginfo, tlSYM("line")));
        info->offset = tlIntCast(tlObjectGet(debuginfo, tlSYM("offset")));
        info->text = tlStringCast(tlObjectGet(debuginfo, tlSYM("text")));
        info->lines = tlListCast(tlObjectGet(debuginfo, tlSYM("lines")));
        info->pos = tlListCast(tlObjectGet(debuginfo, tlSYM("pos")));
        if (!info->line) info->line = tlZero;
        if (!info->offset) info->offset = tlZero;
        if (!info->text) info->text = tlStringEmpty();
        if (!info->lines) info->lines = tlListEmpty();
        if (!info->pos) info->pos = tlListEmpty();
    } else {
        info->line = tlZero;
        info->offset = tlZero;
        info->text = tlStringEmpty();
        info->lines = tlListEmpty();
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

tlNum* tlNumFromBuffer(tlBuffer* buf, int size);
tlNum* readBignum(tlBuffer* buf, int size, const char** error) {
    trace("bignum: %d", size);
    if (tlBufferSize(buf) < size) FAIL("not enough data");
    int oldsize = tlBufferSize(buf);
    tlNum* res = tlNumFromBuffer(buf, size);
    UNUSED(oldsize);
    assert(tlBufferSize(buf) + size == oldsize);
    return res;
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
        case 0xE6: return readBignum(buf, size, error);
        default:
            FAIL("not a sized value");
    }
    fatal("not reached");
    return null;
}

tlHandle readvalue(tlBuffer* buf, tlList* data, tlBModule* mod, const char** error) {
    if (tlBufferSize(buf) < 1) FAIL("not enough data");
    uint8_t b1 = tlBufferReadByte(buf);
    uint8_t type = (uint8_t)(b1 & 0xE0);
    uint8_t size = (uint8_t)(b1 & 0x1F);

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
                union { uint32_t n; float f; } u;
                u.n = 0;
                for (int i = 0; i < 4; i++) {
                    u.n = (u.n << 8) | tlBufferReadByte(buf);
                }
                trace("float: %x %f", u.n, u.f);
                return tlFLOAT(u.f);
            }
            if (size == 9) {
                trace("float size 8");
                union { uint64_t n; double f; } u;
                u.n = 0;
                for (int i = 0; i < 8; i++) {
                    u.n = (u.n << 8) | tlBufferReadByte(buf);
                }
                trace("float: %llx %f", u.n, u.f);
                return tlFLOAT(u.f);
            }
            trace("int size %d", size + 1);
            if (size > 7) FAIL("bad number");
            int64_t val = 0;
            for (int i = 0; i < size; i++) {
                val = (val << 8) | tlBufferReadByte(buf);
            }
            // size is off by one, but last byte is 7 bits + sign
            int last = tlBufferReadByte(buf);
            val = (val << 7) | (last >> 1);
            if (last & 0x1) val = -val;
            trace("read int: %d", val);
            return tlNUM(val); // TODO make sure bytecode fits tlINT or not?
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

    if (tlBufferReadByte(buf) != 't') FAIL("not a module");
    if (tlBufferReadByte(buf) != 'l') FAIL("not a module");
    if (tlBufferReadByte(buf) != '0') FAIL("not a module");
    if (tlBufferReadByte(buf) != '1') FAIL("not a module");

    int size = 0;
    tlHandle direct = readref(buf, null, &size);
    if (direct) {
        assert(size == 0);
        trace("tl01 - %s", tl_str(direct));
        return direct;
    }
    trace("tl01 - %d", size);
    if (size > 10000) FAIL("too many values");

    tlHandle v = null;
    tlList* data = tlListNew(size);
    for (int i = 0; i < size; i++) {
        v = readvalue(buf, data, mod, error);
        if (*error) return null;
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
        tlHandle d = tlListGet(spec, 1);
        if (!d) d = tlNull;
        print("   name=%s, default=%s, lazy=%s", tl_str(tlListGet(spec, 0)), tl_repr(d), tl_bool(tlListGet(spec, 2))? "true":"false");
    }
    print(" </args>");
    print(" <locals>");
    for (int i = 0;; i++) {
        tlHandle name = tlListGet(bcode->localnames, i);
        const char* v = (bcode->localvars && tl_bool(tlListGet(bcode->localvars, i)))? "var " : "";
        if (!name) break;
        print("  %d: %s%s", i, v, tl_str(name));
    }
    print(" </locals>");
    int pc = 0;
    int r = 0; int r2 = 0; int r3 = 0;
    tlHandle o = null;
    UNUSED(o);
    while (true) {
        int opc = pc;
        uint8_t op = ops[pc++];
        switch (op) {
            case OP_END: goto exit;
            case OP_TRUE: case OP_FALSE: case OP_NULL: case OP_UNDEF:
            case OP_ARGS: case OP_THIS:
            case OP_INVOKE:
            case OP_CERR:
                print(" %3d 0x%X %s", opc, op, op_name(op));
                break;
            case OP_INT:
            case OP_ENVTHIS:
            case OP_MCALL: case OP_FCALL: case OP_BCALL: case OP_MCALLS:
            case OP_CCALL: case OP_SCALL:
                r = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d", opc, op, op_name(op), r);
                break;
            case OP_MCALLN: case OP_FCALLN: case OP_BCALLN: case OP_MCALLNS:
                r = pcreadsize(ops, &pc);
                o = pcreadref(ops, &pc, data);
                print(" %3d 0x%X %s: %d n: %s", opc, op, op_name(op), r, tl_repr(o));
                break;
            case OP_SYSTEM:
                o = pcreadref(ops, &pc, data);
                print(" %3d 0x%X %s: %s", opc, op, op_name(op), tl_str(o));
                break;
            case OP_MODULE:
                o = pcreadref(ops, &pc, data);
                print(" %3d 0x%X %s: %s", opc, op, op_name(op), tl_repr(o));
                break;
            case OP_GLOBAL:
                r = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %s(%s)", opc, op, op_name(op),
                        tl_str(tlListGet(links, r)), tl_str(tlListGet(linked, r)));
                break;
            case OP_BIND:
                o = pcreadref(ops, &pc, data);
                print(" %3d 0x%X %s: %s", opc, op, op_name(op), tl_str(o));
                //if (tlBCodeIs(o)) disasm(tlBCodeAs(o));
                break;
            case OP_LOCAL: case OP_VGET: // slot
                r = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d(%s)", opc, op, op_name(op), r, tl_str(tlListGet(bcode->localnames, r)));
                break;
            case OP_ARG:
                r = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d(%s)", opc, op, op_name(op), r, tl_str(tlListGet(tlListGet(bcode->argspec, r), 0)));
                break;
            case OP_STORE: case OP_VSTORE: // slot
                r = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d(%s) <-", opc, op, op_name(op), r, tl_str(tlListGet(bcode->localnames, r)));
                break;
            case OP_RSTORE: case OP_VRSTORE: // result, slot
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d(%s) <- %d", opc, op, op_name(op), r2, tl_str(tlListGet(bcode->localnames, r2)), r);
                break;
            case OP_ENV: case OP_ENVARG: case OP_EVGET: // parent, slot
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d %d", opc, op, op_name(op), r, r2);
                break;
            case OP_EVSTORE: // parent, slot
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d %d <- ", opc, op, op_name(op), r, r2);
                break;
            case OP_EVRSTORE: // parent, result, slot
                r = pcreadsize(ops, &pc); r2 = pcreadsize(ops, &pc); r3 = pcreadsize(ops, &pc);
                print(" %3d 0x%X %s: %d %d <- %d", opc, op, op_name(op), r, r3, r2);
                break;
            default: print("OEPS: %d 0x%X %s", opc, op, op_name(op));
        }
        for (int i = opc + 1; i < pc; i++) print(" %3d 0x%02X", i, ops[i]);
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
            tlHandle v2 = tlObjectValueIter(map, i);
            if (!v2) break;
            tlHandle k = tlObjectKeyIter(map, i);
            print("%s:", tl_str(k));
            bpprint(v2);
        }
        print("}");
        return;
    }
    if (tlListIs(v)) {
        tlList* list = tlListAs(v);
        print("[");
        for (int i = 0;; i++) {
            tlHandle v2 = tlListGet(list, i);
            if (!v2) break;
            bpprint(v2);
        }
        print("]");
        return;
    }
    print("%s", tl_str(v));
}

// for a given pc, find out how many CALLs or GLOBALs we have seen
int tlBCodePosOpsForPc(tlBCode* code, int pc) {
    int pos = 0;
    for (int p = 0; p < pc && p < code->size; p++) {
        uint8_t op = code->code[p];
        if (op == OP_GLOBAL) pos++;
        else if (op == OP_INVOKE) pos++;
        else if ((op & 0xE0) == 0xE0) pos++;
    }
    assert(pos <= (pc + 4) / 2); // not an exact science ... this might trip
    return pos;
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
            case OP_ARGS: break;
            case OP_THIS: break;
            case OP_INT: dreadsize(&code); break;
            case OP_SYSTEM: dreadsize(&code); break;
            case OP_MODULE: dreadref(&code, data); break;
            case OP_GLOBAL: dreadsize(&code); break;
            case OP_ENV:
            case OP_EVGET:
                parent = dreadsize(&code);
                at = dreadsize(&code);
                UNUSED(at);
                // TODO check if parent locals actually support these
                assert(parent >= 0);
                assert(at >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;
            case OP_ENVARG:
                parent = dreadsize(&code);
                at = dreadsize(&code);
                UNUSED(at);
                // TODO check if parent locals actually support these
                assert(parent >= 0);
                assert(at >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;
            case OP_ENVARGS:
            case OP_ENVTHIS:
                parent = dreadsize(&code);
                // TODO check if parent locals actually support these
                assert(parent >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;
            case OP_LOCAL:
            case OP_VGET:
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
            case OP_VSTORE:
                at = dreadsize(&code);
                assert(at >= 0);
                if (at >= locals) FAIL("local store out of range");
                break;
            case OP_RSTORE:
            case OP_VRSTORE:
                dreadsize(&code);
                at = dreadsize(&code);
                assert(at >= 0);
                if (at >= locals) FAIL("local store out of range");
                break;
            case OP_EVSTORE:
                parent = dreadsize(&code);
                at = dreadsize(&code);
                UNUSED(at);
                assert(parent >= 0);
                assert(at >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;
            case OP_EVRSTORE:
                parent = dreadsize(&code);
                dreadsize(&code);
                at = dreadsize(&code);
                UNUSED(at);
                assert(parent >= 0);
                assert(at >= 0);
                if (parent > 200) FAIL("parent out of range");
                break;

            case OP_INVOKE:
                if (depth <= 0) FAIL("invoke without call");
                if (depth > maxdepth) maxdepth = depth;
                depth--;
                break;
            case OP_CERR: break;
            case OP_MCALL:
            case OP_FCALL:
            case OP_BCALL:
            case OP_CCALL:
            case OP_SCALL:
            case OP_MCALLS:
                dreadsize(&code);
                depth++;
                break;
            case OP_MCALLN:
            case OP_FCALLN:
            case OP_BCALLN:
            case OP_MCALLNS:
                dreadsize(&code);
                v = dreadref(&code, data);
                UNUSED(v);
                depth++;
                break;
            default:
                warning("%d - %s", op, op_name(op));
                FAIL("not an opcode");
        }
    }
exit:;
     if (depth != 0) FAIL("call without invoke");
     trace("verified; locals: %d, calldepth: %d, %s", locals, maxdepth, tl_str(bcode));
     bcode->locals = locals;
     bcode->calldepth = maxdepth;
     return tlNull;
}

tlBModule* tlBModuleLink(tlBModule* mod, tlObject* env, const char** error) {
    trace("linking: %p", mod);
    mod->linked = tlListNew(tlListSize(mod->links));
    for (int i = 0; i < tlListSize(mod->links); i++) {
        tlHandle name = tlListGet(mod->links, i);
        assert(tlSymIs_(name));
        tlHandle v = tlObjectGet(env, name);
        if (!v) v = tl_global(name);
        if (!v) v = tlNull; // TODO tlUndef
        trace("linking: %s as %s", tl_str(name), tl_str(v));
        tlListSet_(mod->linked, i, v);
    }
    tlBCodeVerify(mod->body, error);
    if (*error) return null;
    return mod;
}

tlHandle eval_args(tlTask* task, tlArgs* args);
tlHandle eval_lazy(tlTask* task, tlBLazy* lazy);
tlHandle beval(tlTask* task, tlCodeFrame* frame, tlHandle resuming);

static tlHandle afterYieldQuota(tlTask* task, tlFrame* frame, tlHandle res, tlHandle err) {
    if (!res) return null;
    tlTaskPopFrame(task, frame);
    return tlInvoke(task, tlArgsAs(res));
}

tlHandle tlInvokeBinop(tlTask* task, tlArgs* call, bool lhs) {
    tlSym name = tlArgsFn(call);
    assert(tlSymIs_(name));

    tlHandle target = lhs? tlArgsGet(call, 0) : tlArgsGet(call, 1);
    if (!target) target = tlNull;
    tlHandle fn = null;

    tlKind* kind = tl_kind(target);
    if (tlUserObjectIs(target)) fn = userobjectResolve(tlUserObjectAs(target), name);
    if (!fn && kind->klass) fn = tlObjectGet(kind->klass, name);
    if (!fn && kind->cls) fn = classResolve(kind->cls, name);
    if (!fn) return tlUndef();

    tlArgs* args = tlArgsNewNames(3, true, true);
    tlArgsSetFn_(args, fn);
    tlArgsSetTarget_(args, target);
    tlArgsSetMethod_(args, name);
    tlArgsSetNames_(args, TL_NAMED_NULL_LHS_RHS, 2);
    tlArgsSet_(args, 0, tlArgsGet(call, lhs? 1 : 0));
    tlArgsSet_(args, 1, tlArgsGet(call, 0));
    tlArgsSet_(args, 2, tlArgsGet(call, 1));

    return tlInvoke(task, args);
}

tlHandle tlInvoke(tlTask* task, tlArgs* call) {
    assert(!tlTaskHasError(task));
    if (!tlTaskTick(task)) {
        if (tlTaskHasError(task)) return null;

        tlTaskPushResume(task, afterYieldQuota, call);
        tlTaskWaitFor(task, null);
        // TODO move this to a "after stopping", otherwise real threaded envs will pick up task before really stopped
        tlTaskReady(task);
        return null;
    }
    assert(!tlTaskHasError(task));

    tlHandle fn = tlArgsFnInline(call);
    tlHandle target = tlArgsTargetInline(call);
    trace(" %s invoke %s.%s", tl_str(call), tl_str(target), tl_str(fn));
    if (target && tl_kind(target)->locked) {
        if (!tlLockIsOwner(tlLockAs(target), task)) return tlLockAndInvoke(task, call);
    }

    if (tlBSendTokenIs(fn) || tlNativeIs(fn)) {
        if (tlBSendTokenIs(fn)) {
            assert(tlArgsMethodInline(call) == tlBSendTokenAs(fn)->method);
            return tl_kind(target)->send(task, call, tlBSendTokenAs(fn)->safe);
        }
        return tlNativeKind->run(task, tlNativeAs(fn), call);
    }
    if (tlBClosureIs(fn)) {
        return eval_args(task, call);
    }
    if (tlBLazyIs(fn)) {
        tlBLazy* lazy = tlBLazyAs(fn);
        return eval_lazy(task, lazy);
    }
    if (tlBLazyDataIs(fn)) {
        return tlBLazyDataAs(fn)->data;
    }
    // if method call and target is set, field is already resolved and apparently not callable
    if (target && !tlCallableIs(fn)) {
        assert(fn);
        return fn;
    }
    // oldstyle hotel, include {call=(->)}
    if (tlCallableIs(fn)) {
        tlKind* kind = tl_kind(fn);
        if (kind->run) return kind->run(task, fn, call);
    }

    // class support and class.call support
    // TODO if the found fn is not callable, return it
    if (tlClassIs(fn)) {
        tlHandle ctor = tlClassAs(fn)->constructor;
        if (ctor) {
            return tlInvoke(task, tlArgsFrom(call, ctor, null, null));
        }
    }
    tlClass* cls = tlClassFor(fn);
    if (cls) {
        tlHandle newtarget = fn;
        fn = classResolve(cls, s_call);
        if (fn) {
            return tlInvoke(task, tlArgsFrom(call, fn, s_call, newtarget));
        }
    }
    if (tlUserObjectIs(fn)) {
        tlHandle newtarget = fn;
        fn = userobjectResolve(fn, s_call);
        if (fn) {
            return tlInvoke(task, tlArgsFrom(call, fn, s_call, newtarget));
        }
    }
    TL_THROW("'%s' not callable", tl_str(fn));
}

tlHandle tlEval(tlTask* task, tlHandle v) {
    trace("%s", tl_str(v));
    if (tlArgsIs(v)) return tlInvoke(task, tlArgsAs(v));
    return v;
}

static tlBLazy* create_lazy(const uint8_t* ops, const int len, int* ppc, tlArgs* args, tlEnv* locals) {
    // match CALLs with INVOKEs and wrap it for later execution
    // notice bytecode vs literals are such that OP_CALL cannot appear in literals
    int pc = *ppc;
    int start = pc;
    trace("LAZY: %d - %s(%X)", pc, op_name(ops[pc]), ops[pc]);
    if ((ops[pc] & 0xE0) != 0xE0) { // not an OP_CALL
        pc++;
        while ((ops[pc] & 0xC0) != 0xC0) pc++; // upto the next OP
        *ppc = pc;
        // TODO if not mutable, [e]vstore, create a LazyData instead?
        return tlBLazyNew(args, locals, start);
    }
    int depth = 0;
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

static int skip_safe_invokes(const uint8_t* ops, const int len, int pc, int skip) {
    int depth = skip;
    while (pc < len) {
        uint8_t op = ops[pc++];
        trace("SEARCHING: %d - %d - %s(%X)", depth, pc - 1, op_name(op), op);
        if (op == OP_INVOKE) {
            depth--;
            if (!depth) {
                trace("SKIPPED TO: %d", pc);
                return pc;
            }
        } else if ((op & 0xE0) == 0xE0) { // OP_CALL mask
            trace("ADDING DEPTH: %X", op);
            depth++;
        }
    }
    fatal("bytecode invalid");
    return 0;
}

// OP_CCALL always looks like this: CCALL, 0, BIND, index, INVOKE
static int skip_ccall(const uint8_t* ops, const int len, int pc) {
    while (pc < len) {
        uint8_t op = ops[pc++];
        trace("SEARCHING: %d - %s(%X)", pc - 1, op_name(op), op);
        if (op == OP_INVOKE) return pc;
    }
    fatal("bytecode invalid");
    return 0;
}

// we don't allow nesting OP_CCALL and OP_CERR, so just skip one past
static int skip_pcalls(const uint8_t* ops, const int len, int pc) {
    while (pc < len) {
        uint8_t op = ops[pc++];
        trace("SEARCHING: %d - %s(%X)", pc - 1, op_name(op), op);
        if (op == OP_CERR) return pc;
    }
    fatal("bytecode invalid");
    return 0;
}

static tlHandle bmethodResolve(tlHandle target, tlSym method, bool safe) {
    trace("resolve method: %s.%s", tl_str(target), tl_str(method));
    if (tlObjectIs(target)) return objectResolve(target, method);
    if (tlClassIs(target)) return classResolveStatic(tlClassAs(target), method);
    if (tlUserObjectIs(target)) return userobjectResolve(tlUserObjectAs(target), method);
    if (tlUserClassIs(target)) return userclassResolveStatic(tlUserClassAs(target), method);
    assert(target);
    assert(tl_kind(target));
    tlKind* kind = tl_kind(target);
    if (kind->send) return tlBSendTokenNew(method, safe);
    if (kind->klass) return objectResolve(kind->klass, method);
    if (kind->cls) return classResolve(kind->cls, method);
    return null;
}

tlHandle tlFrameEval(tlTask* task, tlCodeFrame* frame) {
    return beval(task, frame, null);
}

// trace frame invocations incase we crash and burn
#define TRACE_EVENT_SIZE 0x10000
#define TRACE_EVENT_MASK 0x0FFFF
static int g_trace_event;
static int g_trace_pc[TRACE_EVENT_SIZE];
static tlTask* g_trace_task[TRACE_EVENT_SIZE];
static tlCodeFrame* g_trace_frame[TRACE_EVENT_SIZE];
static tlArgs* g_trace_args[TRACE_EVENT_SIZE];

void debug_trace_frame(tlTask* task, tlCodeFrame* frame, tlArgs* args) {
    g_trace_event = (g_trace_event + 1) & TRACE_EVENT_MASK;
    g_trace_task[g_trace_event] = task;
    g_trace_pc[g_trace_event] = frame? frame->pc : 0;
    g_trace_frame[g_trace_event] = frame;
    g_trace_args[g_trace_event] = args;
}

void tlDumpTraceEvent(int event) {
    if (!g_trace_task[event]) return;
    int npc = g_trace_frame[event]? g_trace_frame[event]->pc : -1;
    tlBClosure* fn = tlBClosureAs(tlArgsFn(g_trace_args[event]));
    print("task=%p frame=%p pc=%d(%d) function: %s:%s (%d) args=%s", g_trace_task[event],
            g_trace_frame[event], g_trace_pc[event], npc, tl_str(fn->code->mod->name),
            tl_str(fn->code->debuginfo->name), fn->code->size,
            tl_str(g_trace_args[event]));
}

void tlDumpTraceEvents(int count) {
    for (int i = 0; i < count; i++) {
        tlDumpTraceEvent((g_trace_event - i) & TRACE_EVENT_MASK);
    }
}

tlTask* tlDumpTraceGetTask(int count) {
    return g_trace_task[(g_trace_event - count) & TRACE_EVENT_MASK];
}

void tlCodeFrameDump(tlFrame* _frame) {
    tlCodeFrame* frame = (tlCodeFrame*)_frame;
    tlArgs* args = frame->locals->args;
    tlBClosure* closure = tlBClosureAs(args->fn);
    tlBCode* code = closure->code;

    print("PC: %d, CALLER: %p", frame->pc, frame->frame.caller);
    disasm(code);
    print("args: %d", tlArgsSize(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        print("  %d = %s", i, tl_repr(tlArgsGet(args, i)));
    }
    print("locals: %d", tlListSize(code->localnames));
    for (int local = 0; local < tlListSize(code->localnames); local++) {
        tlHandle name = tlListGet(code->localnames, local);
        print("  %d(%s) = %s", local, tl_str(name), tl_repr(tlEnvGet(frame->locals, local)));
    }
}

static tlHandle handleBFrameThrow(tlTask* task, tlCodeFrame* frame, tlHandle throw);
tlHandle eval_resume(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    tlCodeFrame* frame = tlCodeFrameAs(_frame);
    //if (error) return tlCodeFrameHandleError(task, frame, error);
    if (error) return handleBFrameThrow(task, frame, error);
    if (!value) return null;

    return beval(task, frame, value);
}

tlHandle eval_args(tlTask* task, tlArgs* args) {
    trace("%s", tl_str(args));
    tlCodeFrame* frame = tlCodeFrameNew(args, null);

    // init debugger state if needed
    if (tlDebuggerFor(task)) frame->stepping = 1;

    tlTaskPushFrame(task, (tlFrame*)frame);
    return beval(task, frame, null);
}

tlHandle eval_lazy(tlTask* task, tlBLazy* lazy) {
    tlCodeFrame* frame = tlCodeFrameNew(lazy->locals->args, lazy->locals);
    frame->pc = lazy->pc;
    frame->lazy = true;

    tlTaskPushFrame(task, (tlFrame*)frame);
    return beval(task, frame, null);
}

tlHandle beval(tlTask* task, tlCodeFrame* frame, tlHandle resuming) {
    assert(task);
    assert(tlTaskCurrentFrame(task) == (tlFrame*)frame);

    // get current attached debugger, if any
    tlDebugger* debugger = tlDebuggerFor(task);

    trace("task=%p frame=%p pc=%d (lazy:%d) resuming=%s", task, frame, frame->pc, frame->lazy, tl_repr(resuming));

    tlArgs* args = frame->locals->args;
    tlBClosure* closure = tlBClosureAs(args->fn);
    tlBCode* bcode = closure->code;
    tlBModule* mod = bcode->mod;
    tlList* data = mod->data;
    const uint8_t* ops = bcode->code;

    trace("%p '%s' env=%p closure=%p", frame, tl_str(bcode->debuginfo->name), closure->env, closure);
    debug_trace_frame(task, frame, args);

    // program counter, saved and restored into frame
    int pc = frame->pc;

    int calltop = -1; // current call index in the call stack
    tlArgs* call = null; // current call, if any
    int arg = 0; // current argument to a call

    uint8_t op = 0; // current opcode
    tlHandle v = resuming? resuming : tlNull; // tmp result register

    // we did an invoke, and paused the task, but now the result is ready; see OP_INVOKE
    // or we did a resolve, which paused
    if (resuming || frame->stepping == 2) {
        // we need to find the current calltop/call/arg
        calltop = 0;
        while (calltop < bcode->calldepth) {
            if (!frame->calls[calltop].call) break;
            calltop += 1;
        }
        calltop -= 1; // compensate
        if (calltop >= 0) {
            call = frame->calls[calltop].call;
            arg = frame->calls[calltop].at;
            assert(call);
        }
        trace("resuming: %p -- %d(%d) %p", frame, calltop, bcode->calldepth, call);
        if (frame->stepping) {
            frame->stepping = 0;
            goto again;
        }
        goto resume;
    }

again:;
    if (debugger && frame->stepping == 1) {
        frame->pc = pc;
        frame->calls[calltop].at = arg;
        task->value = v;
        if (!tlDebuggerStep(debugger, task, frame)) {
            frame->stepping = 2;
            trace("pausing for debugger");
            return null;
        }
    }
    if (debugger && !frame->stepping) frame->stepping = 1;

    if (tlBCallIsLazy(call, arg)) {
        trace("LAZY %d[%d]", calltop, arg - 2);
        v = create_lazy(ops, bcode->size, &pc, args, frame->locals);
        tlBCallAdd_(call, v, arg++);
        goto again;
    }

    if (pc < 0 || pc > bcode->size) fatal("bad pc: %d", pc);
    frame->pc = pc;
    op = ops[pc++];
    if (!op) {
        assert(v);
        tlTaskPopFrame(task, (tlFrame*)frame);
        return v; // OP_END
    }
    assert(op & 0xC0);

    // is it a call
    if (op & 0x20) {
        int size = pcreadsize(ops, &pc);
        trace("%d call op: 0x%X %s - %d", pc, op, op_name(op), size);

        if (op == OP_CCALL) {
            if (!tl_bool(v)) {
                pc = skip_ccall(ops, bcode->size, pc); // predicate is false, skip this clause
                goto again;
            }
        }

        // create a new call
        if (calltop >= 0) {
            assert(frame->calls[calltop].call == call);
            frame->calls[calltop].at = arg;
            trace("push call: %d: %d %s", calltop, arg, tl_str(call));
        }
        arg = 0;
        call = tlArgsNewNames(size, op & 0x10, (op & 0x7) == 0 || op == OP_SCALL); // size, hasNames, isMethod
        if (op == OP_SCALL) tlArgsMakeSetter_(call);
        calltop++;
        assert(calltop >= 0 && calltop < bcode->calldepth);
        assert(frame->calls[calltop].call == null);
        frame->calls[calltop].at = 0;
        frame->calls[calltop].call = call;
        frame->calls[calltop].safe = false;
        frame->calls[calltop].bcall = false;
        frame->calls[calltop].ccall = op == OP_CCALL;

        // load names if it is a named call
        if (op & 0x10) {
            int at = pcreadsize(ops, &pc);
            tlList* names = tlListAs(tlListGet(data, at));
            trace("set names: %s", tl_repr(names));
            tlArgsSetNames_(call, names, 0);
#ifdef HAVE_ASSERT
            for (int i = 0, l = tlListSize(names); i < l; i++) {
                assert(tlSymIs_(tlListGet(names, i)) || tlListGet(names, i) == tlNull);
            }
#endif
        }
        // for non methods, skip target
        if (op & 0x07 && op != OP_SCALL) {
            tlBCallAdd_(call, null, arg++);
        }
        // safe methods, mark as such
        if (op & 0x08) {
            trace("safe method call");
            frame->calls[calltop].safe = true;
        }
        // bcall, mark as such
        if (op == OP_BCALL) {
            trace("binary call");
            frame->calls[calltop].bcall = true;
        }
        goto again;
    }

    trace("%d data op: 0x%X %s", pc, op, op_name(op));
    int at;
    int depth;

    switch (op) {
        case OP_INVOKE: {
            assert(tlTaskCurrentFrame(task) == (tlFrame*)frame);
            assert(call);
            assert(arg - 2 == tlArgsRawSizeInline(call)); // must be done with args here

            tlArgs* invoke = call;
            trace("%p invoke: %s %s", frame, tl_str(invoke), tl_str(invoke->fn));
            if (frame->calls[calltop].ccall) {
                pc = skip_pcalls(ops, bcode->size, pc); // about to execute a true clause, skip any others
            }
            frame->pc = pc;

            // pop the frame's local callstack
            frame->calls[calltop].at = 0;
            frame->calls[calltop].call = null;
            assert(!frame->bcall);
            frame->bcall = frame->calls[calltop].bcall;
            calltop--;
            if (calltop >= 0) {
                call = frame->calls[calltop].call;
                arg = frame->calls[calltop].at;
                assert(call);
                assert(arg >= 0);
                trace("pop call: %d: %d %s", calltop, arg, tl_str(call));
            } else {
                call = null;
                arg = 0;
            }

            // check if it is a goto call, and if so, remove all stack frames and jump to the invoke
            if (call && call->fn == g_goto_native) {
                // check arguments to goto
                // TODO perhaps leave one "goto" frame, to ensure return semantic:
                // "return foo()" returns single value, "goto foo()" might return many values
                // and to allow "goto 10, foo()", though those forms defeat goto's purpose
                if (tlArgsRawSizeInline(call) != 2) TL_THROW("goto requires a single argument");
                unwindForGoto(task, tl_int(tlArgsGet(call, 0)), tlNull);
                tlTaskPushResume(task, resumeBCall, invoke);
                return null;
            }

            if (frame->bcall) {
                // for compat, only do this if ArgsFn is an actual operator string; TODO remove this
                if (!tlSymIs_(tlArgsFnInline(invoke))) {
                    frame->bcall = 0;
                    v = tlInvoke(task, invoke);
                } else {
                    // we invoke binops twice, if lhs returns undefined, but have to be prepared to suspend
                    // this is part one, below is part two
                    v = tlInvokeBinop(task, invoke, true);
                    frame->invoke = invoke;
                    assert(frame->bcall == 1);
                }
            } else {
                v = tlInvoke(task, invoke);
            }
            if (!v) return null;
            trace("%p after: %d", frame, pc);
            assert(tlTaskCurrentFrame(task) == (tlFrame*)frame);
            break;
        }
        case OP_CERR: {
            TL_THROW_NORETURN("no branch taken");
            // TODO remove this duplication
            if (calltop >= 0) frame->calls[calltop].at = arg; // mark as current
            trace("pause attach: %s pc: %d", tl_str(frame), frame->pc);
            return null;
        }
        case OP_SYSTEM: {
            // TODO make useful or remove ...
            at = pcreadsize(ops, &pc);
            UNUSED(at);
            //tlHandle n = tlListGet(data, at);
            //if (n == s_createobject) {
            v = tlEnvLocalObject((tlFrame*)frame);
            //}
            //fatal("syscall: %s", tl_str(n));
            //v = tlNull;
            break;
        }
        case OP_MODULE:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            trace("data %s", tl_str(v));
            break;
        case OP_GLOBAL:
            at = pcreadsize(ops, &pc);
            v = tlListGet(mod->linked, at);
            trace("linked %s (%s)", tl_str(v), tl_str(tlListGet(mod->links, at)));
            if (v == tlUnknown) {
                TL_THROW_NORETURN("'%s' is not known", tl_str(tlListGet(mod->links, at)));
                // TODO remove this duplication
                if (calltop >= 0) frame->calls[calltop].at = arg; // mark as current
                trace("pause attach: %s pc: %d", tl_str(frame), frame->pc);
                return null;
            }
            break;
        case OP_ENVARG: {
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlEnv* parent = tlEnvGetParentAt(closure->env, depth);
            v = tlEnvGetArg(parent, at);
            assert(v);
            trace("envarg[%d][%d] -> %s", depth, at, tl_str(v));
            break;
        }
        case OP_ENV: {
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlEnv* parent = tlEnvGetParentAt(closure->env, depth);
            assert(parent);
            v = tlEnvGetVar(parent, at); // TODO only for/until OP_EVGET
            assert(v);
            if (!v) v = tlNull;
            trace("env[%d][%d] -> %s", depth, at, tl_str(v));
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
            v = tlEnvGetVar(frame->locals, at); // TODO only for/until OP_VGET
            assert(v); // or throw? or undefined?
            if (!v) v = tlNull;
            trace("local %d -> %s", at, tl_str(v));
            break;
        case OP_ARGS:
            v = args;
            trace("args: %s", tl_str(v));
            break;
        case OP_ENVARGS: {
            depth = pcreadsize(ops, &pc);
            tlEnv* parent = tlEnvGetParentAt(closure->env, depth);
            v = parent->args;
            assert(v);
            trace("envargs[%d] -> %s", depth, tl_str(v));
            break;
        }
        case OP_THIS: {
            // TODO is this needed, compiler can figure out the exact this
            v = tlArgsTargetInline(args);
            tlEnv* env = frame->locals;
            trace("this: %s (%s)", tl_str(args), tl_str(v));
            while (!v && env) {
                env = env->parent;
                if (env) v = tlArgsTargetInline(env->args);
            }
            //assert(v);
            if (!v) v = tlNull;
            break;
        }
        case OP_ENVTHIS: {
            depth = pcreadsize(ops, &pc);
            tlEnv* env = tlEnvGetParentAt(closure->env, depth);
            v = tlArgsTargetInline(env->args);
            trace("this: %s (%s)", tl_str(args), tl_str(v));
            while (!v && env) {
                env = env->parent;
                if (env) v = tlArgsTargetInline(env->args);
            }
            //assert(v);
            if (!v) v = tlNull;
            trace("envthis[%d] -> %s", depth, tl_str(v));
            break;
        }
        case OP_BIND:
            at = pcreadsize(ops, &pc);
            v = tlListGet(data, at);
            v = tlBClosureNew(tlBCodeAs(v), frame->locals);
            trace("%d bind %s", pc, tl_str(v));
            break;
        case OP_STORE:
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            tlEnvSet_(frame->locals, at, tlFirst(v));
            trace("store %d <- %s", at, tl_str(v));
            break;
        case OP_RSTORE: {
            int rat = pcreadsize(ops, &pc);
            tlHandle res = tlResultGet(v, rat);
            at = pcreadsize(ops, &pc);
            tlEnvSet_(frame->locals, at, res);
            trace("result %d <- %s (%d %s)", at, tl_str(res), rat, tl_str(v));
            break;
        }
        // TODO these all need to be thread safe, when we support multithreading again
        case OP_VGET: {
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            v = tlEnvGetVar(frame->locals, at);
            if (!v) v = tlNull; // TODO tlUndefined or throw?
            trace("vget %d -> %s", at, tl_str(v));
            break;
        }
        case OP_VSTORE: {
            at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < bcode->locals);
            tlEnvSetVar_(frame->locals, at, tlFirst(v));
            trace("vstore %d <- %s", at, tl_str(v));
            break;
        }
        case OP_VRSTORE: {
            int rat = pcreadsize(ops, &pc);
            tlHandle res = tlResultGet(v, rat);
            at = pcreadsize(ops, &pc);
            tlEnvSetVar_(frame->locals, at, res);
            trace("rvstore %d <- %s (%d %s)", at, tl_str(res), rat, tl_str(v));
            break;
        }
        case OP_EVGET: {
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlEnv* parent = tlEnvGetParentAt(closure->env, depth);
            assert(parent);
            v = tlEnvGetVar(parent, at);
            assert(v);
            trace("env[%d][%d] -> %s", depth, at, tl_str(v));
            break;
        }
        case OP_EVSTORE: {
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlEnv* parent = tlEnvGetParentAt(closure->env, depth);
            assert(parent);
            tlEnvSetVar_(parent, at, v);
            trace("env[%d][%d] <- %s", depth, at, tl_str(v));
            break;
        }
        case OP_EVRSTORE: {
            int rat = pcreadsize(ops, &pc);
            tlHandle res = tlResultGet(v, rat);
            depth = pcreadsize(ops, &pc);
            at = pcreadsize(ops, &pc);
            assert(depth >= 0 && at >= 0);
            tlEnv* parent = tlEnvGetParentAt(closure->env, depth);
            assert(parent);
            tlEnvSet_(parent, at, res);
            trace("env[%d][%d] <- %s (%d %s)", depth, at, tl_str(res), rat, tl_str(v));
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

    // check if we are in a operator (binary call)
    if (frame->bcall) {
        trace("after bcall: %d", frame->bcall);
        tlArgs* invoke = frame->invoke;
        assert(invoke && tlSymIs_(tlArgsFnInline(invoke)));
        if (frame->bcall == 1 && tlUndefinedIs(v)) {
            // lhs.operator(lhs, rhs) returned undefined, we try again for rhs
            frame->bcall = 2;
            v = tlInvokeBinop(task, invoke, false);
            if (!v) return null;
        }
        if (frame->bcall == 2 && tlUndefinedIs(v)) {
            // both sides returned undefined, we throw
            frame->bcall = 0;
            TL_THROW("operator not defined for '%s %s %s'",
                tl_str(tlArgsGet(invoke, 0)), tl_str(tlArgsFn(invoke)), tl_str(tlArgsGet(invoke, 1)));
        }
        if (frame->bcall) {
            // either lhs, or rhs returned a result, reset the state and fall into normal processing
            assert(!tlUndefinedIs(v));
            frame->invoke = null;
            frame->bcall = 0;
        }
    }
    assert(!frame->bcall);

    if (!call) {
        if (frame->lazy) {
            tlTaskPopFrame(task, (tlFrame*)frame);
            return v; // if we were evaulating a lazy call, and we are back at "top", OP_END
        }
        goto again;
    }

    // set the data to the call
    trace("load: %s[%d] = %s", tl_str(call), arg, tl_str(v));
    tlBCallAdd_(call, tlFirst(v), arg++);

    // resolve method at object ...
    tlHandle target = tlArgsTargetInline(call);
    if (arg == 2 && target && tlStringIs(call->fn)) {
        tlSym mname = tlSymFromString(call->fn);
        bool safe = frame->calls[calltop].safe;
        tlHandle method = bmethodResolve(target, mname, safe);
        trace("method resolve: %s %s", tl_str(call->fn), tl_str(method));
        if (!method && !safe) {
            TL_THROW_NORETURN("'%s' is not a property of '%s'", tl_str(mname), tl_str(target));
            // TODO remove this duplication
            if (calltop >= 0) frame->calls[calltop].at = arg; // mark as current
            trace("pause attach: %s pc: %d", tl_str(frame), frame->pc);
            return null;
        }
        if (!method) {
            assert(safe); // object?method but method does not exist
            int skip = 1;
            frame->calls[calltop].call = null;
            calltop -= 1;
            while (calltop >= 0) {
                if (frame->calls[calltop].at > 0) break;
                // skip any calls we are the target off
                frame->calls[calltop].call = null;
                skip += 1;
                calltop -= 1;
            }
            // skip forward in bytecode until so many invokes are skipped
            trace("skipping: %d, top: %d, pc: %d", skip, calltop, pc);
            //disasm(bcode);
            pc = skip_safe_invokes(ops, bcode->size, pc, skip);
            trace("skipped: %d, top: %d, pc: %d", skip, calltop, pc);

            v = tlNull;
            if (calltop >= 0) {
                arg = frame->calls[calltop].at;
                call = frame->calls[calltop].call;
                trace("popped callstack: %s %d %s", tl_str(call), arg, tl_str(call->fn));
                assert(call->fn);
                assert(arg > 0);
                tlBCallAdd_(call, tlNull, arg++);
            } else {
                arg = 0;
                call = null;
                if (frame->lazy) {
                    tlTaskPopFrame(task, (tlFrame*)frame);
                    return v; // if we were evaulating a lazy call, and we are back at "top", OP_END
                }
            }
            goto again;
        }
        tlArgsSetMethod_(call, mname);
        call->fn = method;
    }
    goto again;
}

// get stack frame info from a single beval frame
void tlCodeFrameGetInfo(tlFrame* _frame, tlString** file, tlString** function, tlInt* line) {
    tlCodeFrame* frame = tlCodeFrameAs(_frame);

    tlArgs* args = frame->locals->args;
    assert(args);
    tlBClosure* closure = tlBClosureAs(args->fn);
    tlBCode* code = closure->code;
    tlBModule* mod = code->mod;

    *file = mod->name;
    tlBDebugInfo* info = code->debuginfo;
    assert(info);

    *function = info->name;
    *line = info->line;
    if (!(*file)) *file = _t_unknown;
    if (!(*function)) *function = _t_anon;

    // figure out exact line from bytecode position
    tlList* lines = mod->body->debuginfo->lines;
    tlList* pos = info->pos;
    int posops = tlBCodePosOpsForPc(code, frame->pc) - 1;
    if (posops < 0) return;
    tlHandle byte = tlListGet(pos, posops);
    if (!byte) return;
    if (!tlIntIs(byte)) return;
    int l = 0;
    int size = tlListSize(lines);
    for (; l < size; l++) {
        assert(tlIntIs(tlListGet(lines, l)));
        if (tlListGet(lines, l) > byte) break;
    }
    if (l >= size) return;
    trace("FOUND LINE: %d (%d, %s)", l, posops, tl_str(byte));
    *line = tlINT(l + 1);
}

static tlHandle _env_locals(tlTask* task, tlArgs* args) {
    tlCodeFrame* frame = tlCodeFrameAs(task->stack);
    tlBCode* code = tlBClosureAs(frame->locals->args->fn)->code;
    tlHashMap* res = tlHashMapNew();

    const uint8_t* ops = code->code;
    int pc = 0;
    while (pc < frame->pc) {
        uint8_t op = ops[pc++];
        if (op == OP_STORE) {
            trace("OP: %s", op_name(op));
            int at = pcreadsize(ops, &pc);
            assert(at >= 0 && at < code->locals);
            tlHandle name = tlListGet(code->localnames, at);
            tlHandle v = tlEnvGet(frame->locals, at);
            trace("store %s(%d) = %s", tl_str(name), at, tl_str(v));
            tlHashMapSet(res, name, v);
        } else {
            trace("OP: %s", op_name(op));
        }
    }
    return res;
}

// TODO every env needs an indication of where its was "closed" compared to its parent
tlHandle _env_current(tlTask* task, tlArgs* args) {
    tlCodeFrame* frame = tlCodeFrameAs(task->stack);
    tlHashMap* res = tlHashMapNew();

    tlEnv* env = frame->locals;
    while (env) {
        for (int i = tlListSize(env->names) - 1; i >= 0; i--) {
            tlHandle name = tlListGet(env->names, i);
            tlHandle v = tlEnvGet(env, i);
            if (!v) continue;
            if (tlHashMapGet(res, name)) continue;
            tlHashMapSet(res, name, v);
        }
        env = env->parent;
    }

    tlObject* gs = tlVmGlobals(tlVmCurrent(task));
    for (int i = 0;; i++) {
        tlHandle key = null;
        tlHandle value = null;
        tlObjectKeyValueIter(gs, i, &key, &value);
        if (!key) break;
        if (!tlHashMapGet(res, key)) tlHashMapSet(res, key, value);
    }

    LHashMapIter* iter = lhashmapiter_new(globals);
    for (int i = 0;; i++) {
        void* key = null;
        tlHandle value = null;
        lhashmapiter_get(iter, &key, &value);
        if (!key) break;
        key = tlSYM((const char*)key);
        if (!tlHashMapGet(res, key)) tlHashMapSet(res, key, value);
        lhashmapiter_next(iter);
    }

    return res;
}

static tlHandle resumeBCall(tlTask* task, tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("running resume a call");
    if (throw) return null;
    if (!res) return null;
    tlTaskPopFrame(task, frame);
    tlArgs* call = tlArgsAs(res);
    return eval_args(task, call);
}

static tlHandle handleBFrameThrow(tlTask* task, tlCodeFrame* frame, tlHandle error) {
    assert(tlTaskCurrentFrame(task) != (tlFrame*)frame);
    assert(tlTaskHasError(task));

    tlHandle handler = frame->handler;
    tlArgs* call = null;
    if (tlBClosureIs(handler)) {
        trace("invoke closure as exception handler: %s %s(%s)", tl_str(frame), tl_str(handler), tl_str(error));
        call = tlArgsNew(1);
        tlArgsSetFn_(call, handler);
        tlArgsSet_(call, 0, error);
    }
    if (tlCallableIs(handler)) {
        // operate with old style hotel
        trace("invoke callable as exception handler: %s %s(%s)", tl_str(frame), tl_str(handler), tl_str(error));
        call = tlArgsNew(1);
        tlArgsSetFn_(call, handler);
        tlArgsSet_(call, 0, error);
    }
    if (call) {
        tlTaskClearError(task, tlNull);
        tlTaskEval(task, call);
        return null;
    }

    trace("returing handler value as handled: %s %s", tl_str(frame), tl_str(handler));
    return tlTaskClearError(task, handler);
}

static tlHandle resumeBFrame(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    trace("running resuming from a frame: %s value=%s, error=%s", tl_str(_frame), tl_str(value), tl_str(error));
    tlCodeFrame* frame = tlCodeFrameAs(_frame);
    if (error) {
        if (frame->handler) return handleBFrameThrow(task, frame, error);
        return null;
    }
    if (!value) return null;
    assert(tlTaskValue(task) == value);
    return beval(task, frame, value);
}

tlHandle beval_module(tlTask* task, tlBModule* mod) {
    tlBClosure* fn = tlBClosureNew(mod->body, null);
    tlArgs* call = tlArgsNew(0);
    call->fn = fn;
    return eval_args(task, call);
}

tlBModule* tlBModuleFromBuffer(tlBuffer* buf, tlString* name, const char** error) {
    tlBModule* mod = tlBModuleNew(name);
    deserialize(buf, mod, error);
    if (*error) return null;
    assert(mod->body);

    mod->linked = tlListNew(tlListSize(mod->links));
    for (int i = 0; i < tlListSize(mod->linked); i++) tlListSet_(mod->linked, i, tlUnknown);

    tlBCodeVerify(mod->body, error);
    if (*error) return null;

    return mod;
}

tlTask* tlBModuleCreateTask(tlVm* vm, tlBModule* mod, tlArgs* args) {
    //fatal("broken");
    tlBClosure* fn = tlBClosureNew(mod->body, null);
    args->fn = fn;

    tlTask* task = tlTaskNew(vm, vm->locals);
    assert(task->state == TL_STATE_INIT);

    task->value = args;
    task->stack = tlFrameAlloc(resumeBCall, sizeof(tlFrame));
    tlTaskStart(task);
    return task;
}

static tlHandle _Module_new(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("require a buffer as args[1]");
    tlString* name = tlStringCast(tlArgsGet(args, 1));

    const char* error = null;
    tlBModule* mod = tlBModuleFromBuffer(buf, name, &error);
    if (error) TL_THROW("invalid bytecode: %s", error);
    return mod;
}

static tlHandle _disasm(tlTask* task, tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    if (!mod) {
        tlCodeFrame* frame = tlCodeFrameAs(tlTaskCurrentFrame(task));
        mod = tlBClosureAs(frame->locals->args->fn)->code->mod;
    }
    for (int i = 0;; i++) {
        tlHandle data = tlListGet(mod->data, i);
        if (!data) break;
        if (tlBCodeIs(data)) disasm(tlBCodeAs(data));
    }
    return tlNull;
}

static tlHandle _module_frame(tlTask* task, tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    if (!mod) TL_THROW("run requires a module");
    tlBClosure* fn = tlBClosureNew(mod->body, null);

    tlArgs* as;
    tlHandle _as = tlArgsGet(args, 1);
    if (!_as || _as == tlNull) {
        as = tlClone(tlArgsEmpty());
    } else if (tlArgsIs(_as)) {
        as = tlClone(tlArgsAs(_as));
    } else {
        TL_THROW("frame requires Args as args[2]");
    }
    as->fn = fn;
    tlCodeFrame* frame = tlCodeFrameNew(as, null);
    tlCodeFrame* link = tlCodeFrameCast(tlArgsGet(args, 2));
    if (link) tlEnvLink_(frame->locals, link->locals, mod->body->localvars);
    return frame;
}

//. object Frame: a context for executing code, like from eval

//. run: run the current frame, once running, you cannot reuse this frame
static tlHandle _frame_run(tlTask* task, tlArgs* args) {
    TL_TARGET(tlCodeFrame, frame);
    if (frame->pc != 0) TL_THROW("cannot reuse frames");

    tlTask* other = tlTaskCast(tlArgsGet(args, 0));
    if (!other) {
        tlTaskPushFrame(task, (tlFrame*)frame);
        return beval(task, frame, null);
    }

    if (other->state != TL_STATE_INIT) TL_THROW("cannot reuse a task");
    other->value = tlNull;
    other->stack = (tlFrame*)frame;
    other->state = TL_STATE_INIT;
    tlTaskStart(other);
    return tlNull;
}

static tlHandle _frame_locals(tlTask* task, tlArgs* args) {
    TL_TARGET(tlCodeFrame, frame);

    tlEnv* env = frame->locals;
    tlList* names = env->names;
    tlObject* map = tlObjectNew(0);
    for (int i = 0; i < tlListSize(names); i++) {
        tlHandle name = tlListGet(names, i);
        tlHandle v = tlEnvGet(env, i);
        if (!name) continue;
        if (!v) continue;
        map = tlObjectSet(map, tlSymAs(name), v);
    }
    return map;
}

//. size: the amount of values stored in this frame
static tlHandle _frame_size(tlTask* task, tlArgs* args) {
    TL_TARGET(tlCodeFrame, frame);
    return tlINT(tlListSize(frame->locals->names));
}

//. get(at): return the value stored at position #at, returning four things: value, name, type, position
static tlHandle _frame_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlCodeFrame, frame);
    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    tlList* localvars = closure->code->localvars;

    tlEnv* env = frame->locals;
    int at = at_offset(tlArgsGet(args, 0), tlListSize(env->names));

    if (at < 0) return tlUndef();
    tlHandle value = tlEnvGet(env, at);
    if (!value) value = tlEnvGetVar(env, at); // try again as var
    if (!value) value = tlNull;

    tlHandle name = tlListGet(env->names, at);
    assert(tl_bool(name));
    tlHandle type = (localvars && tl_bool(tlListGet(localvars, at)))? s_var : tlNull;
    return tlResultFrom(value, name, type, tlNull, null);
}

//. allStored: a property set to true if the last statement of the code is an assignment
//. > "x = 10".eval(frame=true).allStored == true
static tlHandle _frame_allStored(tlTask* task, tlArgs* args) {
    tlCodeFrame* frame = tlCodeFrameCast(tlArgsTargetInline(args));
    if (!frame) TL_THROW("run requires a frame");

    tlBClosure* closure = tlBClosureAs(frame->locals->args->fn);
    tlBCode* code = closure->code;

    int op = code->code[code->size - 3];
    switch (op) {
        case OP_STORE: case OP_RSTORE: return tlTrue;
        case OP_VSTORE: case OP_VRSTORE: return tlTrue;
        case OP_EVSTORE: case OP_EVRSTORE: return tlTrue;
        default: return tlFalse;
    }
}

static tlHandle _module_run(tlTask* task, tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    if (!mod) TL_THROW("run requires a module");

    tlArgs* as = null;
    tlHandle* _as = tlArgsGet(args, 1);
    if (!_as || _as == tlNull) {
        as = tlClone(tlArgsEmpty());
    } else if (tlArgsIs(_as)) {
        as = tlClone(tlArgsAs(_as));
    } else if (tlListIs(_as)) {
        tlList* list = tlListAs(_as);
        int size = tlListSize(list);
        as = tlArgsNew(size);
        for (int i = 0; i < size; i++) {
            tlArgsSet_(as, i, tlListGet(list, i));
        }
    }
    if (!as) TL_THROW("require an Args or List as args[2], not: %s", tl_str(_as));

    tlTask* other = tlTaskCast(tlArgsGet(args, 2));
    tlCodeFrame* frame = tlCodeFrameCast(tlArgsGet(args, 3));
    if (frame && frame->pc != 0) TL_THROW("cannot reuse frames");
    if (!frame) {
        tlBClosure* fn = tlBClosureNew(mod->body, null);
        as->fn = fn; // as is a copy ...
        frame = tlCodeFrameNew(as, null);
    }
    trace("MODULE RUN: %s %s %s", tl_str(other), tl_str(frame), tl_str(as));

    if (!other) {
        tlTaskPushFrame(task, (tlFrame*)frame);
        return beval(task, frame, null);
    }

    if (other->state != TL_STATE_INIT) TL_THROW("cannot reuse a task");
    other->value = tlNull;
    other->stack = (tlFrame*)frame;
    other->state = TL_STATE_INIT;
    tlTaskStart(other);
    return tlNull;
}

static tlHandle _module_links(tlTask* task, tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    if (!mod) TL_THROW("require a module");
    return mod->links;
}

static tlHandle _module_link(tlTask* task, tlArgs* args) {
    tlBModule* mod = tlBModuleCast(tlArgsGet(args, 0));
    tlList* links = tlListCast(tlArgsGet(args, 1));
    if (!mod) TL_THROW("expect a module as args[1]");
    if (!links) TL_THROW("expect a list as args[2]");
    if (tlListSize(links) != tlListSize(mod->links)) TL_THROW("arg[2].size != module.links.size");

    trace("link: %s", tl_repr(links));
    mod->linked = links;
    return mod;
}

static tlHandle _unknown(tlTask* task, tlArgs* args) {
    return tlUnknown;
}

// TODO use symbols
void module_overwrite_(tlBModule* mod, tlString* key, tlHandle value) {
    tlList* links = mod->links;
    for (int i = 0; i < tlListSize(links); i++) {
        if (tlHandleEquals(key, tlListGet(links, i))) {
            trace("OVERWRITING: %d %s %s", i, tl_str(key), tl_str(value));
            tlListSet_(mod->linked, i, value);
            return;
        }
    }
}

static tlHandle __list(tlTask* task, tlArgs* args) {
    tlList* list = tlListNew(tlArgsSize(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlListSet_(list, i, tlArgsGet(args, i));
    }
    trace("%s", tl_repr(list));
    return list;
}

static tlHandle __object(tlTask* task, tlArgs* args) {
    tlObject* object = tlClone(tlObjectAs(tlArgsGet(args, 0)));
    for (int i = 1; i < tlArgsSize(args); i += 2) {
        tlObjectSet_(object, tlArgsGet(args, i), tlArgsGet(args, i + 1));
    }
    trace("%s", tl_repr(object));
    return object;
}

static tlHandle __map(tlTask* task, tlArgs* args) {
    tlObject* object = tlClone(tlObjectAs(tlArgsGet(args, 0)));
    for (int i = 1; i < tlArgsSize(args); i += 2) {
        tlObjectSet_(object, tlArgsGet(args, i), tlArgsGet(args, i + 1));
    }
    trace("%s", tl_repr(object));
    return tlMapFromObject_(object);
}

static tlHandle __return(tlTask* task, tlArgs* args) {
    int deep = tl_int(tlArgsGet(args, 0));
    trace("RETURN SCOPES: %d", deep);

    // create a return value
    tlHandle res = tlNull;
    if (tlArgsSize(args) == 2) res = tlArgsGet(args, 1);
    if (tlArgsSize(args) > 2) res = tlResultFromArgsSkipOne(args);

    // TODO we should return our scoped function, not the first frame
    tlCodeFrame* scopeframe = tlCodeFrameAs(tlTaskCurrentFrame(task));
    assert(scopeframe->locals);
    tlEnv* target = tlEnvGetParentAt(scopeframe->locals, deep);

    tlFrame* frame = (tlFrame*)scopeframe;
    while (frame) {
        trace("%p", frame);
        if (tlCodeFrameIs(frame)) {
            tlCodeFrame* bframe = tlCodeFrameAs(frame);
            trace("found a bframe: %s{.pc=%d,.env=%s) env=%s", tl_str(bframe), bframe->pc, tl_str(bframe->locals), tl_str(bframe->locals));
            // lazy frames have same env as target ... but we ignore them
            if (!bframe->lazy && bframe->locals == target) {
                scopeframe = bframe;
                break;
            }
            // we keep track of lexically related frames, for when the to return to target is not on stack (captured blocks)
            if (!bframe->lazy && bframe->locals == scopeframe->locals->parent) {
                scopeframe = tlCodeFrameAs(frame);
                trace("found a scope frame: %s.pc=%d (%d)", tl_str(frame), scopeframe->pc, deep);
            }
        }
        frame = frame->caller;
    }
    assert(scopeframe);
    return tlTaskUnwindFrame(task, ((tlFrame*)scopeframe)->caller, res);
}

static void unwindForGoto(tlTask* task, int deep, tlHandle value) {
    trace("UNWIND GOTO SCOPES: %d", deep);

    tlCodeFrame* scopeframe = tlCodeFrameAs(tlTaskCurrentFrame(task));
    assert(scopeframe->locals);
    tlEnv* target = tlEnvGetParentAt(scopeframe->locals, deep);

    tlFrame* frame = (tlFrame*)scopeframe;
    while (frame) {
        trace("%p", frame);
        if (tlCodeFrameIs(frame)) {
            tlCodeFrame* bframe = tlCodeFrameAs(frame);
            trace("found a bframe: %s{.pc=%d,.env=%s) env=%s", tl_str(bframe), bframe->pc, tl_str(bframe->locals), tl_str(bframe->locals));
            // negative pc means a lazy frame, has same locals and bcall as target ... but we ignore it
            if (!bframe->lazy && bframe->locals == target) {
                scopeframe = bframe;
                break;
            }
            // we keep track of lexically related frames, for when the to return to target is not on stack (captured blocks)
            if (!bframe->lazy && bframe->locals == scopeframe->locals->parent) {
                scopeframe = tlCodeFrameAs(frame);
                trace("found a scope frame: %s.pc=%d (%d)", tl_str(frame), scopeframe->pc, deep);
            }
        }
        frame = frame->caller;
    }
    assert(scopeframe);

    // now we unwind the stack, releasing all locks and such
    tlTaskUnwindFrame(task, ((tlFrame*)scopeframe)->caller, value);
}

// if just goto is called, it is because there was no invoke
static tlHandle __goto(tlTask* task, tlArgs* args) {
    if (tlArgsRawSize(args) != 2) TL_THROW("goto requires a single argument");
    tlHandle value = tlOR_NULL(tlArgsGet(args, 1));
    unwindForGoto(task, tl_int(tlArgsGet(args, 0)), value);
    return null;
}

static tlHandle _identity(tlTask* task, tlArgs* args) {
    tlHandle v = tlArgsGet(args, 0);
    return v? v: tlNull;
}

void install_bcatch(tlFrame* _frame, tlHandle handler) {
    trace("installing exception handler: %s %s", tl_str(_frame), tl_str(handler));
    tlCodeFrameAs(_frame)->handler = handler;
}

tlHandle _bcatch(tlTask* task, tlArgs* args) {
    tlCodeFrame* frame = tlCodeFrameAs(tlTaskCurrentFrame(task));
    tlHandle handler = tlArgsBlock(args);
    if (!handler) handler = tlArgsGet(args, 0);
    if (!handler) handler = tlNull;
    trace("installing exception handler: %s %s", tl_str(frame), tl_str(handler));
    frame->handler = handler;
    return tlNull;
}

static tlHandle _bclosure_call(tlTask* task, tlArgs* args) {
    trace("bclosure.call");
    tlBClosure* fn = tlBClosureCast(tlArgsTargetInline(args));
    if (!fn) TL_THROW(".call expects a Function as this");

    tlHandle a1 = tlArgsGet(args, 0);
    if (!a1) a1 = tlArgsEmpty();
    if (!tlArgsIs(a1)) TL_THROW(".call expects an Args or nothing, not: %s", tl_str(a1));
    tlArgs* from = tlArgsAs(a1);
    tlHandle target = tlArgsGetNamed(args, s_this);
    tlSym method = tlSymCast(tlArgsGetNamed(args, s_method));

    int size = tlArgsRawSize(from);
    tlList* names = tlArgsNames(from);

    tlArgs* call = tlArgsNewNames(size, names != null, target || method);

    if (names) tlArgsSetNames_(call, names, tlArgsNamedSize(from));
    if (target) tlArgsSetTarget_(call, target);
    if (method) tlArgsSetMethod_(call, method);
    tlArgsSetFn_(call, fn);

    for (int i = 0; i < size; i++) {
        tlArgsSet_(call, i, tlArgsGetRaw(from, i));
    }
    assert(tlBClosureIs(call->fn));
    return eval_args(task, call);
}
static tlHandle runBClosure(tlTask* task, tlHandle _fn, tlArgs* args) {
    trace("bclosure.run");
    //fatal("broken");
    args->fn = _fn;
    assert(tlBClosureIs(args->fn));
    return eval_args(task, args);
}
static tlHandle _bclosure_name(tlTask* task, tlArgs* args) {
    TL_TARGET(tlBClosure, fn);
    return tlOR_NULL(tlBCodeName(fn->code));
}
static tlHandle _bclosure_file(tlTask* task, tlArgs* args) {
    TL_TARGET(tlBClosure, fn);
    tlBModule* mod = fn->code->mod;
    return mod->name;
}
static tlHandle _bclosure_line(tlTask* task, tlArgs* args) {
    TL_TARGET(tlBClosure, fn);
    if (fn->code->debuginfo) return fn->code->debuginfo->line;
    return tlNull;
}
static tlHandle _bclosure_args(tlTask* task, tlArgs* args) {
    TL_TARGET(tlBClosure, fn);
    return fn->code->argspec;
}
static tlHandle _bclosure_postable(tlTask* task, tlArgs* args) {
    TL_TARGET(tlBClosure, fn);
    return tlOR_NULL(fn->code->debuginfo->pos);
}
static tlHandle _bclosure_bytecode(tlTask* task, tlArgs* args) {
    TL_TARGET(tlBClosure, fn);
    int opcount = 0;
    const uint8_t* ops = fn->code->code;
    assert(ops);
    for (int i = 0; i < fn->code->size; i++) {
        uint8_t op = ops[i];
        if ((op & 0xC0) != 0xC0) continue;
        opcount += 1;
    }

    int at = 0;
    tlList* list = tlListNew(opcount);
    for (int i = 0; i < fn->code->size; i++) {
        uint8_t op = ops[i];
        if ((op & 0xC0) != 0xC0) continue;
        tlListSet_(list, at, tlSYM(op_name(op)));
        at += 1;
    }
    assert(tlListGet(list, opcount - 1));
    return list;
}

static const char* bclosureToString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Function@%p %s>", v, tlStringData(tlBCodeName(tlBClosureAs(v)->code)));
    return buf;
}

static tlHandle _blazy_call(tlTask* task, tlArgs* args) {
    trace("blazy.call");
    tlBLazy* lazy = tlBLazyAs(tlArgsTargetInline(args));
    return eval_lazy(task, lazy);
}
static tlHandle runBLazy(tlTask* task, tlHandle fn, tlArgs* args) {
    trace("blazy.run");
    tlBLazy* lazy = tlBLazyAs(fn);
    return eval_lazy(task, lazy);
}

static tlHandle _blazydata_call(tlTask* task, tlArgs* args) {
    trace("blazydata.call");
    return tlBLazyDataAs(tlArgsTargetInline(args))->data;
}
static tlHandle runBLazyData(tlTask* task, tlHandle fn, tlArgs* args) {
    trace("blazydata.run");
    return tlBLazyDataAs(fn)->data;
}

static const tlNativeCbs __bcode_natives[] = {
    { "_Module_new", _Module_new },
    { "_module_run", _module_run },
    { "_module_links", _module_links },
    { "_module_link", _module_link },
    { "_module_frame", _module_frame },
    { "_unknown", _unknown },
    { "__list", __list },
    { "__map", __map },
    { "__object", __object },
    { "return", __return },
    { "_env_locals", _env_locals },
    { "_env_current", _env_current },
    { "_disasm", _disasm },
    { 0, 0 }
};

void bcode_init() {
    assert(OP_INVOKE < OP_MCALL);
    tl_register_natives(__bcode_natives);
    g_goto_native = tlNativeNew(__goto, tlSYM("goto"));
    tl_register_global("goto", g_goto_native);
    g_identity = tlNativeNew(_identity, tlSYM("identity"));
    tl_register_global("identity", g_identity);

    _tlBLazyKind.klass = tlClassObjectFrom("call", _blazy_call, null);
    _tlBLazyDataKind.klass = tlClassObjectFrom("call", _blazydata_call, null);
    _tlBLazyKind.run = runBLazy;
    _tlBLazyDataKind.run = runBLazyData;

    _tlBClosureKind.toString = bclosureToString;
    _tlBClosureKind.run = runBClosure;
    _tlBClosureKind.klass = tlClassObjectFrom(
        "call", _bclosure_call,
        "file", _bclosure_file,
        "name", _bclosure_name,
        "line", _bclosure_line,
        "args", _bclosure_args,
        "postable", _bclosure_postable,
        "bytecode", _bclosure_bytecode,
    null);

    tlFrameKind->klass = tlClassObjectFrom(
        "run", _frame_run,
        "size", _frame_size,
        "get", _frame_get,
        "locals", _frame_locals,
        "allStored", _frame_allStored,
    null);

    INIT_KIND(tlBModuleKind);
    INIT_KIND(tlBDebugInfoKind);
    INIT_KIND(tlBCodeKind);
    INIT_KIND(tlBClosureKind);
    INIT_KIND(tlBLazyKind);
    INIT_KIND(tlBLazyDataKind);
    INIT_KIND(tlBSendTokenKind);
}

