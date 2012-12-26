// author: Onne Gorter, license: MIT (see license.txt)
// args, passed to all functions, contains the evaluated values (call is the unevaluated args)
//
// TODO ideally, add block directly in struct
// TODO make most of these "flags", saves space
// TODO remove fn ... should be possible

#include "trace-off.h"

static tlKind _tlArgsKind;
tlKind* tlArgsKind = &_tlArgsKind;

struct tlArgs {
    tlHead head;
    tlHandle fn;
    tlHandle target;
    tlSym msg;
    tlMap* map;
    tlList* list;
};

static tlArgs* v_args_empty;

tlArgs* tlArgsNewNames(int size, tlSet* names) {
    if (!names) names = _tl_set_empty;
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs));
    args->list = tlListNew(size - tlSetSize(names));
    args->map = tlMapNew(names);
    return args;
}

tlArgs* tlArgsNew(tlList* list, tlMap* map) {
    if (!list) list = _tl_emptyList;
    if (!map) map = _tl_emptyMap;
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs));
    args->list = list;
    args->map = map;
    return args;
}

tlHandle tlArgsTarget(tlArgs* args) { assert(tlArgsIs(args)); return args->target; }
tlHandle tlArgsMsg(tlArgs* args) { assert(tlArgsIs(args)); return args->msg; }
tlHandle tlArgsFn(tlArgs* args) { assert(tlArgsIs(args)); return args->fn; }

int tlArgsSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlListSize(args->list);
}
int tlArgsMapSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlMapSize(args->map);
}
tlList* tlArgsList(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->list;
}
tlMap* tlArgsMap(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->map;
}
tlHandle tlArgsGet(tlArgs* args, int at) {
    assert(tlArgsIs(args));
    return tlListGet(args->list, at);
}
tlHandle tlArgsMapGet(tlArgs* args, tlSym name) {
    assert(tlArgsIs(args));
    return tlMapGetSym(args->map, name);
}
tlHandle tlArgsBlock(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlArgsMapGet(args, s_block);
}

void tlArgsSetFn_(tlArgs* args, tlHandle fn) {
    assert(tlArgsIs(args));
    assert(!args->fn);
    args->fn = fn;
}
void tlArgsSet_(tlArgs* args, int at, tlHandle v) {
    assert(tlArgsIs(args));
    tlListSet_(args->list, at, v);
}
void tlArgsMapSet_(tlArgs* args, tlSym name, tlHandle v) {
    assert(tlArgsIs(args));
    tlMapSetSym_(args->map, name, v);
}

static tlHandle _args_size(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlINT(tlArgsSize(as));
}
static tlHandle _args_get(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    int at = tl_int_or(tlArgsGet(args, 0), -1);
    if (at == -1) TL_THROW("Expected an index");
    tlHandle res = tlArgsGet(as, at);
    return (res)? res : tlUndefined;
}
static tlHandle _args_this(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    tlHandle res = tlArgsTarget(as);
    return (res)? res : tlUndefined;
}
static tlHandle _args_msg(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    tlHandle res = tlArgsMsg(as);
    return (res)? res : tlUndefined;
}
static tlHandle _args_block(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    tlHandle res = tlArgsBlock(as);
    return (res)? res : tlUndefined;
}
static tlHandle _args_map(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    tlHandle res = tlArgsMap(as);
    return (res)? res : tlUndefined;
}
static tlHandle _args_names(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    tlHandle res = tlMapToObject(tlArgsMap(as));
    return (res)? res : tlUndefined;
}
static tlHandle _args_slice(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");

    tlList* list = as->list;
    int size = tlListSize(list);
    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), size);
    return tlListSlice(list, first, last);
}

const char* _ArgsToText(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Args@%p %d %d>", v, tlArgsSize(tlArgsAs(v)), tlArgsMapSize(tlArgsAs(v)));
    return buf;
}

static tlKind _tlArgsKind = {
    .name = "Args",
    .toText = _ArgsToText,
};

static void args_init() {
    _tlArgsKind.klass = tlClassMapFrom(
            "size", _args_size,
            "get", _args_get,
            "slice", _args_slice,

            "this", _args_this,
            "msg", _args_msg,
            "block", _args_block,

            "map", _args_map,
            "names", _args_names,
            null
    );
    v_args_empty = tlArgsNew(null, null);
}

