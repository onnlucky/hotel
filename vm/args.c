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
    tlObject* map;
    tlList* list;
};

static tlArgs* v_args_empty;

tlArgs* tlArgsNewNames(int size, tlSet* names) {
    if (!names) names = _tl_set_empty;
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs));
    args->list = tlListNew(size - tlSetSize(names));
    args->map = tlObjectNew(names);
    return args;
}

tlArgs* tlArgsNew(tlList* list, tlObject* map) {
    if (!list) list = _tl_emptyList;
    if (!map) map = tlObjectEmpty();
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
    return tlObjectSize(args->map);
}
tlList* tlArgsList(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->list;
}
tlObject* tlArgsObject(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->map;
}
tlMap* tlArgsMap(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlMapFromObject(args->map);
}
tlHandle tlArgsGet(const tlArgs* args, int at) {
    return tlListGet(args->list, at);
}
tlHandle tlArgsMapGet(tlArgs* args, tlSym name) {
    assert(tlArgsIs(args));
    return tlObjectGetSym(args->map, name);
}
tlHandle tlArgsBlock(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlArgsMapGet(args, s_block);
}

void tlArgsSetTarget_(tlArgs* args, tlHandle target) {
    assert(tlArgsIs(args));
    assert(!args->target);
    args->target = target;
}
void tlArgsSetFn_(tlArgs* args, tlHandle fn) {
    assert(tlArgsIs(args));
    assert(!args->fn);
    args->fn = fn;
}
void tlArgsSetMsg_(tlArgs* args, tlSym msg) {
    assert(tlArgsIs(args));
    assert(!args->msg);
    args->msg = msg;
}
void tlArgsSet_(tlArgs* args, int at, tlHandle v) {
    assert(tlArgsIs(args));
    tlListSet_(args->list, at, v);
}
void tlArgsMapSet_(tlArgs* args, tlSym name, tlHandle v) {
    assert(tlArgsIs(args));
    tlObjectSet_(args->map, name, v);
}

static tlHandle _args_size(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlINT(tlArgsSize(as));
}
static tlHandle _args_first(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlMAYBE(tlArgsGet(as, 0));
}
static tlHandle _args_last(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlMAYBE(tlArgsGet(as, tlArgsSize(args) - 1));
}
static tlHandle _args_get(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));

    tlHandle v = tlArgsGet(args, 0);
    if (!v || tlIntIs(v) || tlFloatIs(v)) {
        int at = at_offset(tlArgsGet(args, 0), tlArgsSize(as));
        return tlMAYBE(tlArgsGet(as, at));
    }
    if (tlSymIs(v)) {
        return tlMAYBE(tlArgsMapGet(as, tlSymAs(v)));
    }
    TL_THROW("Expected an index or name");
}
static tlHandle _args_this(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsTarget(as);
    return tlMAYBE(res);
}
static tlHandle _args_msg(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsMsg(as);
    return tlMAYBE(res);
}
static tlHandle _args_block(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsBlock(as);
    return tlMAYBE(res);
}
static tlHandle _args_names(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsObject(as);
    return tlMAYBE(res);
}
static tlHandle _args_namesmap(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsMap(as);
    return tlMAYBE(res);
}
static tlHandle _args_slice(tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));

    tlList* list = as->list;
    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), -1);
    int offset;
    int len = sub_offset(first, last, tlListSize(list), &offset);
    return tlListSub(list, offset, len);
}

const char* _ArgstoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Args@%p %d %d>", v, tlArgsSize(tlArgsAs(v)), tlArgsMapSize(tlArgsAs(v)));
    return buf;
}

static tlKind _tlArgsKind = {
    .name = "Args",
    .toString = _ArgstoString,
};

static void args_init() {
    v_args_empty = tlArgsNew(null, null);
    _tlArgsKind.klass = tlClassObjectFrom(
            "this", _args_this,
            "msg", _args_msg,
            "block", _args_block,
            "names", _args_names,
            "namesmap", _args_namesmap,
            //"raw", _args_raw,

            //"hash", _args_hash,
            "size", _args_size,
            "get", _args_get,
            "first", _args_first,
            "last", _args_last,
            "slice", _args_slice,
            "each", null,
            "map", null,
            "find", null,
            "reduce", null,
            "filter", null,
            "flatten", null,
            "join", null,
            null
    );
    tlObject* constructor = tlClassObjectFrom(
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s__methods, _tlArgsKind.klass);
    tl_register_global("Args", constructor);
}

