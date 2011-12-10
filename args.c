// author: Onne Gorter, license: MIT (see license.txt)
// args, passed to all functions, contains the evaluated values (call is the unevaluated args)
//
// TODO ideally, add block directly in struct
// TODO make most of these "flags", saves space
// TODO remove fn ... should be possible

#include "trace-off.h"

static tlClass _tlArgsClass;
tlClass* tlArgsClass = &_tlArgsClass;

struct tlArgs {
    tlHead head;
    tlValue fn;
    tlValue target;
    tlSym msg;
    tlMap* map;
    tlList* list;
};

static tlArgs* v_args_empty;

tlArgs* tlArgsNewNames(tlTask* task, int size, tlSet* names) {
    if (!names) names = _tl_set_empty;
    tlArgs* args = tlAlloc(task, tlArgsClass, sizeof(tlArgs));
    args->list = tlListNew(task, size - tlSetSize(names));
    args->map = tlMapNew(task, names);
    return args;
}

tlArgs* tlArgsNew(tlTask* task, tlList* list, tlMap* map) {
    if (!list) list = _tl_emptyList;
    if (!map) map = _tl_emptyMap;
    tlArgs* args = tlAlloc(task, tlArgsClass, sizeof(tlArgs));
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

static tlValue _args_size(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlINT(tlArgsSize(as));
}
static tlValue _args_get(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    int at = tl_int_or(tlArgsGet(args, 0), -1);
    if (at == -1) TL_THROW("Expected an index");
    tlValue res = tlArgsGet(as, at);
    if (!res) return tlUndefined;
    return res;
}
static tlValue _args_block(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    tlValue res = tlArgsBlock(as);
    if (!res) return tlUndefined;
    return res;
}
static tlValue _args_map(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlArgsMap(as);
}
static tlValue _args_names(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    return tlArgsMap(as);
}

const char* _ArgsToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Args@%p %d %d>", v, tlArgsSize(tlArgsAs(v)), tlArgsMapSize(tlArgsAs(v)));
    return buf;
}

static tlClass _tlArgsClass = {
    .name = "Args",
    .toText = _ArgsToText,
};

static void args_init() {
    _tlArgsClass.map = tlClassMapFrom(
            "size", _args_size,
            "get", _args_get,
            "block", _args_block,
            "map", _args_map,
            "names", _args_names,
            null
    );
    v_args_empty = tlArgsNew(null, null, null);
}

