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
    tlValue fn;
    tlValue target;
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

tlValue tlArgsTarget(tlArgs* args) { assert(tlArgsIs(args)); return args->target; }
tlValue tlArgsMsg(tlArgs* args) { assert(tlArgsIs(args)); return args->msg; }
tlValue tlArgsFn(tlArgs* args) { assert(tlArgsIs(args)); return args->fn; }

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
tlValue tlArgsGet(tlArgs* args, int at) {
    assert(tlArgsIs(args));
    return tlListGet(args->list, at);
}
tlValue tlArgsMapGet(tlArgs* args, tlSym name) {
    assert(tlArgsIs(args));
    return tlMapGetSym(args->map, name);
}
tlValue tlArgsBlock(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlArgsMapGet(args, s_block);
}

void tlArgsSetFn_(tlArgs* args, tlValue fn) {
    assert(tlArgsIs(args));
    assert(!args->fn);
    args->fn = fn;
}
void tlArgsSet_(tlArgs* args, int at, tlValue v) {
    assert(tlArgsIs(args));
    tlListSet_(args->list, at, v);
}
void tlArgsMapSet_(tlArgs* args, tlSym name, tlValue v) {
    assert(tlArgsIs(args));
    tlMapSetSym_(args->map, name, v);
}

static tlValue _args_size(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlINT(tlArgsSize(as));
}
static tlValue _args_get(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    int at = tl_int_or(tlArgsGet(args, 0), -1);
    if (at == -1) TL_THROW("Expected an index");
    tlValue res = tlArgsGet(as, at);
    if (!res) return tlUndefined;
    return res;
}
static tlValue _args_block(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    tlValue res = tlArgsBlock(as);
    if (!res) return tlUndefined;
    return res;
}
static tlValue _args_map(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlArgsMap(as);
}
static tlValue _args_names(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlArgsMap(as);
}
static tlValue _args_slice(tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");

    tlList* list = as->list;
    int size = tlListSize(list);
    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), size);
    return tlListSlice(list, first, last);
}

const char* _ArgsToText(tlValue v, char* buf, int size) {
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
            "block", _args_block,
            "map", _args_map,
            "names", _args_names,
            "slice", _args_slice,
            null
    );
    v_args_empty = tlArgsNew(null, null);
}

