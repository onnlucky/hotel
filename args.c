// args, passed to all functions, contains the evaluated values (call is the unevaluated args)

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
    if (!names) names = v_set_empty;
    tlArgs* args = tlAlloc(task, tlArgsClass, sizeof(tlArgs));
    args->list = tllist_new(task, size - tlset_size(names));
    args->map = tlmap_new(task, names);
    return args;
}

tlArgs* tlArgsNew(tlTask* task, tlList* list, tlMap* map) {
    if (!list) list = v_list_empty;
    if (!map) map = _tl_emptyMap;
    tlArgs* args = tlAlloc(task, tlArgsClass, sizeof(tlArgs));
    args->list = list;
    args->map = map;
    return args;
}

tlValue tlArgsTarget(tlArgs* args) { return args->target; }
tlValue tlArgsMsg(tlArgs* args) { return args->msg; }

int tlArgsSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return tllist_size(args->list);
}
int tlArgsMapSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlmap_size(args->map);
}
tlValue tlArgsFn(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->fn;
}
tlList* tlArgsList(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->list;
}
tlMap* tlArgsMap(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->map;
}
tlValue tlArgsAt(tlArgs* args, int at) {
    assert(tlArgsIs(args));
    return tllist_get(args->list, at);
}
tlValue tlArgsMapGet(tlArgs* args, tlSym name) {
    assert(tlArgsIs(args));
    return tlmap_get_sym(args->map, name);
}

void tlArgsSetFn_(tlArgs* args, tlValue fn) {
    assert(tlArgsIs(args));
    assert(!args->fn);
    args->fn = fn;
}
void tlArgsSetAt_(tlArgs* args, int at, tlValue v) {
    assert(tlArgsIs(args));
    tllist_set_(args->list, at, v);
}
void tlArgsMapSet_(tlArgs* args, tlSym name, tlValue v) {
    assert(tlArgsIs(args));
    tlmap_set_sym_(args->map, name, v);
}

static tlPause* _ArgsSize(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    TL_RETURN(tlINT(tlArgsSize(as)));
}
static tlPause* _ArgsMap(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    TL_RETURN(tlArgsMap(as));
}
static tlPause* _ArgsNames(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsCast(tlArgsTarget(args));
    if (!as) TL_THROW("Expected a args object");
    TL_RETURN(tlArgsMap(as));
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
            "size", _ArgsSize,
            "map", _ArgsMap,
            "names", _ArgsNames,
            null
    );
    v_args_empty = tlArgsNew(null, null, null);
}

