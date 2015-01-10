// author: Onne Gorter, license: MIT (see license.txt)

#include "trace-off.h"

// var.c
tlHandle tlVarSet(tlVar*, tlHandle);
tlHandle tlVarGet(tlVar*);

static tlString* _t_unknown;
static tlString* _t_anon;
static tlString* _t_native;

tlBModule* g_boot_module;

static tlKind _tlResultKind = { .name = "Result" };
tlKind* tlResultKind;
struct tlResult {
    tlHead head;
    intptr_t size;
    tlHandle data[];
};

tlResult* tlResultFromArgs(tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * size);
    res->size = size;
    for (int i = 0; i < size; i++) {
        res->data[i] = tlArgsGet(args, i);
    }
    return res;
}

tlResult* tlResultFromArgsSkipOne(tlArgs* args) {
    int size = tlArgsSize(args);
    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * (size - 1));
    res->size = size - 1;
    for (int i = 1; i < size; i++) {
        res->data[i - 1] = tlArgsGet(args, i);
    }
    return res;
}

tlResult* tlResultFrom(tlHandle v1, ...) {
    va_list ap;
    int size = 0;

    //assert(false);
    va_start(ap, v1);
    for (tlHandle v = v1; v; v = va_arg(ap, tlHandle)) size++;
    va_end(ap);

    tlResult* res = tlAlloc(tlResultKind, sizeof(tlResult) + sizeof(tlHandle) * (size));
    res->size = size;

    va_start(ap, v1);
    int i = 0;
    for (tlHandle v = v1; v; v = va_arg(ap, tlHandle), i++) res->data[i] = v;
    va_end(ap);

    trace("RESULTS: %d", size);
    return res;
}

tlHandle tlResultGet(tlHandle v, int at) {
    assert(at >= 0);
    if (!tlResultIs(v)) {
        if (at == 0) return v;
        if (tlUndefinedIs(v)) return v;
        return tlNull;
    }
    tlResult* result = tlResultAs(v);
    if (at < result->size) return result->data[at];
    return tlNull;
}

// return the first value from a list of results, or just the value passed in
tlHandle tlFirst(tlHandle v) {
    if (!v) return v;
    if (!tlResultIs(v)) return v;
    tlResult* result = tlResultAs(v);
    if (0 < result->size) return result->data[0];
    return tlNull;
}

void tlCodeFrameGetInfo(tlFrame* frame, tlString** file, tlString** function, tlInt* line);
INTERNAL void tlFrameGetInfo(tlFrame* frame, tlString** file, tlString** function, tlInt* line) {
    assert(file); assert(function); assert(line);
    if (tlCodeFrameIs(frame)) {
        tlCodeFrameGetInfo(frame, file, function, line);
    } else {
        *file = tlStringEmpty();
        *function = _t_native;
        *line = tlZero;
    }
}

INTERNAL tlHandle evalArgs(tlTask* task, tlArgs* args) {
    assert(tlTaskIs(task));
    tlHandle fn = tlArgsFn(args);
    trace("%p %s -- %s", args, tl_str(args), tl_str(fn));
    tlKind* kind = tl_kind(fn);
    if (kind->run) return kind->run(task, fn, args);
    TL_THROW("'%s' not callable", tl_str(fn));
    return null;
}

tlHandle tlEvalArgsFn(tlTask* task, tlArgs* args, tlHandle fn) {
    trace("%s %s", tl_str(fn), tl_str(args));
    assert(tlCallableIs(fn));
    tlArgs* nargs = tlClone(args);
    nargs->fn = fn;
    return evalArgs(task, nargs);
}

tlHandle tlEvalArgsTarget(tlTask* task, tlArgs* args, tlHandle target, tlHandle fn) {
    assert(tlCallableIs(fn));
    assert(args);
    int size = tlArgsRawSize(args);
    tlList* names = tlArgsNames(args);

    tlArgs* nargs = tlArgsNewNames(size, names != null, true);

    if (names) tlArgsSetNames_(nargs, names, tlArgsNamedSize(args));
    nargs->fn = fn;
    tlArgsSetTarget_(nargs, target);
    tlArgsSetMethod_(nargs, tlArgsMethod(args));

    for (int i = 0; i < size; i++) {
        tlArgsSet_(nargs, i, tlArgsGetRaw(args, i));
    }
    return evalArgs(task, nargs);
}

// Various high level eval stuff, will piggyback on the task given
tlString* tltoString(tlHandle v) {
    if (tlSymIs(v)) return tlStringAs(v);
    if (tlStringIs(v)) return v;
    // TODO actually invoke toString ...
    return tlStringFromCopy(tl_str(v), 0);
}

INTERNAL tlHandle _bcatch(tlTask* task, tlArgs* args);
INTERNAL void install_bcatch(tlFrame* frame, tlHandle handler);
bool tlCodeFrameIs(tlHandle);

INTERNAL tlHandle _catch(tlTask* task, tlArgs* args) {
    tlFrame* frame = tlTaskCurrentFrame(task);
    tlHandle handler = tlArgsBlock(args);
    trace("%p.handler = %s", frame, tl_str(handler));
    install_bcatch(frame, handler);
    return tlNull;
}

// TODO not so sure about this, user objects which return something on .call are also callable ...
// TODO and maybe an object.class.call is defined or from a _get it returns something callable ...
bool tlCallableIs(tlHandle v) {
    if (!tlRefIs(v)) return false;
    if (tlObjectIs(v)) return tlObjectGetSym(v, s_call) != null;
    tlKind* kind = tl_kind(v);
    if (kind->call) return true;
    if (kind->run) return true;
    if (kind->klass && tlObjectGetSym(kind->klass, s_call)) return true;
    return false;
}

// implement fn.call ... pretty generic
// TODO cat all lists, join all maps, allow for this=foo msg=bar for sending?
INTERNAL tlHandle _call(tlTask* task, tlArgs* args) {
    tlHandle* fn = tlArgsTarget(args);
    tlArgs* nargs = tlArgsGet(args, 0);
    return tlEvalArgsFn(task, nargs, fn);
}

static tlHandle _bufferFromFile(tlTask* task, tlArgs* args) {
    tlString* file = tlStringCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a file name");
    tlBuffer* buf = tlBufferFromFile(tlStringData(file));
    if (!buf) TL_THROW("unable to read file: '%s'", tlStringData(file));
    return buf;
}

static tlHandle _stringFromFile(tlTask* task, tlArgs* args) {
    tlString* file = tlStringCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a file name");
    tlBuffer* buf = tlBufferFromFile(tlStringData(file));
    if (!buf) TL_THROW("unable to read file: '%s'", tlStringData(file));

    tlBufferWriteByte(buf, 0);
    return tlStringFromTake(tlBufferTakeData(buf), 0);
}

INTERNAL tlHandle _install(tlTask* task, tlArgs* args) {
    trace("install: %s", tl_str(tlArgsGet(args, 1)));
    tlObject* map = tlObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("expected a Map");
    tlSym sym = tlSymCast(tlArgsGet(args, 1));
    if (!sym) TL_THROW("expected a Sym");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("expected a Value");
    // TODO some safety here?
    tlObjectSet_(map, sym, val);
    return tlNull;
}

INTERNAL void module_overwrite_(tlBModule* mod, tlString* key, tlHandle value);
INTERNAL tlHandle _install_global(tlTask* task, tlArgs* args) {
    trace("install global: %s %s", tl_str(tlArgsGet(args, 0)), tl_str(tlArgsGet(args, 1)));
    tlString* key = tlStringCast(tlArgsGet(args, 0));
    if (!key) TL_THROW("expected a String");
    tlHandle val = tlArgsGet(args, 1);
    if (!val) TL_THROW("expected a Value");
    tl_register_global(tlStringData(key), val);

    if (g_boot_module) {
        module_overwrite_(g_boot_module, key, val);
    }

    return tlNull;
}

static tlHandle _install_constructor(tlTask* task, tlArgs* args) {
    trace("install %s.constructor = %s", tl_str(tlArgsGet(args, 0)), tl_str(tlArgsGet(args, 1)));
    tlClass* cls = tlClassCast(tlArgsGet(args, 0));
    if (!cls) TL_THROW("expected a Class");
    tlHandle val = tlArgsGet(args, 1);
    if (!val) TL_THROW("expected a Value");
    cls->constructor = val;
    return tlNull;
}

static tlHandle _install_method(tlTask* task, tlArgs* args) {
    trace("install: %s.%s = %s", tl_str(tlArgsGet(args, 0)), tl_str(tlArgsGet(args, 1)), tl_str(tlArgsGet(args, 2)));
    tlClass* cls = tlClassCast(tlArgsGet(args, 0));
    if (!cls) TL_THROW("expected a Class");
    tlSym sym = tlSymCast(tlArgsGet(args, 1));
    if (!sym) TL_THROW("expected a Sym");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("expected a Value");
    tlObjectSet_(cls->methods, sym, val);
    return tlNull;
}

static const tlNativeCbs __eval_natives[] = {
    { "_bufferFromFile", _bufferFromFile },
    { "_stringFromFile", _stringFromFile },
    { "_with_lock", _with_lock },

    { "_String_cat", _String_cat },
    { "_List_clone", _List_clone },
    { "_Map_clone", _Map_clone },
    { "_Object_clone", _Object_clone },

    { "_install", _install },
    { "_install_global", _install_global },
    { "_install_constructor", _install_constructor },
    { "_install_method", _install_method },

    { "catch", _catch },

    { 0, 0 }
};

static void eval_init() {
    INIT_KIND(tlResultKind);
    INIT_KIND(tlFrameKind);

    _t_unknown = tlSTR("<unknown>");
    _t_anon = tlSTR("<anon>");
    _t_native = tlSTR("<native>");

    tl_register_natives(__eval_natives);
}

