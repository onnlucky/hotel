// author: Onne Gorter, license: MIT (see license.txt)
//
// args, passed to all functions

#include "trace-off.h"

tlKind* tlArgsKind;

struct tlArgs {
    tlHead head;
    int size;
    int nsize;
    tlHandle target;
    tlSym msg;
    tlHandle fn;
    tlList* names;
    tlHandle args[];
};

void dumpArgs(tlArgs* call) {
    tlList* names = call->names;
    print("args: %d (%d %d)", call->size + call->nsize, call->size, call->nsize);
    print("%s", tl_str(call->fn));
    print("%s", tl_str(call->target));
    print("%s", tl_str(call->msg));
    for (int i = 0; i < call->size + call->nsize; i++) {
        print("%d %s %s", i, tl_str(call->args[i]), tl_str(names? tlListGet(names, i) : tlNull));
    }
}

static tlArgs* _tl_emptyArgs;
tlArgs* tlArgsEmpty() { return _tl_emptyArgs; }

tlArgs* tlArgsNewNew(int size) {
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * size);
    args->size = size;
    return args;
}

tlArgs* tlArgsNewNames(int size, tlSet* names) {
    fatal("shouldn't use this");
    int nsize = tlSetSize(names);
    size += nsize;
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * size);
    args->size = size;
    args->nsize = nsize;
    return args;
}

tlArgs* tlArgsNewSize(int size, int nsize) {
    fatal("shouldn't use this");
    size += nsize;
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * size);
    args->size = size;
    args->nsize = nsize;
    return args;
}

tlArgs* tlArgsNew(tlList* list, tlObject* map) {
    fatal("shouldn't use this");
    return null;
}

tlArgs* tlArgsNewFromListMap(tlList* list, tlObject* map) {
    if (!list) list = tlListEmpty();
    if (!map) map = tlObjectEmpty();
    int lsize = tlListSize(list);
    int nsize = tlObjectSize(map);
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * (lsize + nsize));
    args->size = lsize + nsize;
    args->nsize = nsize;
    for (int i = 0; i < lsize; i++) {
        args->args[i] = tlListGet(list, i);
    }
    if (nsize > 0) {
        args->names = tlListNew(lsize + nsize);
        for (int i = 0; i < lsize; i++) tlListSet_(args->names, i, tlNull);
        for (int i = 0; i < nsize; i++) {
            tlHandle name;
            tlHandle value;
            tlObjectKeyValueIter(map, i, &name, &value);
            tlListSet_(args->names, lsize + i, name);
            args->args[lsize + i] = value;
        }
    }
    return args;
}

tlHandle tlArgsTarget(tlArgs* args) { assert(tlArgsIs(args)); return args->target; }
tlHandle tlArgsMsg(tlArgs* args) { assert(tlArgsIs(args)); return args->msg; }
tlHandle tlArgsFn(tlArgs* args) { assert(tlArgsIs(args)); return args->fn; }

int tlArgsSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->size - args->nsize;
}
int tlArgsMapSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->nsize;
}
tlList* tlArgsList(tlArgs* args) {
    assert(tlArgsIs(args));
    tlList* list = tlListNew(args->size - args->nsize);
    int named = 0;
    for (int i = 0; i < args->size; i++) {
        tlHandle name = args->names? tlListGet(args->names, i) : tlNull;
        if (name != tlNull) { named++; continue; }
        tlListSet_(list, i - named, args->args[i]);
    }
    assert(tlListGet(list, args->size - args->nsize - 1));
    return list;
}
tlObject* tlArgsObject(tlArgs* args) {
    assert(tlArgsIs(args));
    if (!args->names) return tlObjectEmpty();

    tlSet* set = tlSetNew(args->nsize);
    for (int i = 0; i < args->size; i++) {
        tlHandle name = tlListGet(args->names, i);
        if (name == tlNull) continue;
        tlSetAdd_(set, name);
    }

    tlObject* object = tlObjectNew(set);
    for (int i = 0; i < args->size; i++) {
        tlHandle name = tlListGet(args->names, i);
        if (name == tlNull) continue;
        tlObjectSet_(object, name, args->args[i]);
    }
    assert(tlObjectSize(object) == args->nsize);
    return object;
}
tlMap* tlArgsMap(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlMapFromObject_(tlArgsObject(args));
}
tlHandle tlArgsGet(const tlArgs* args, int at) {
    if (at < 0 || at >= args->size - args->nsize) return null;
    if (!args->names) return args->args[at];

    int target = -1;
    for (int i = 0; i < args->size; i++) {
        if (tlListGet(args->names, i) == tlNull) target++;
        if (target == at) return args->args[i];
    }
    fatal("precondition failed");
    return null;
}
tlHandle tlArgsMapGet(tlArgs* args, tlSym name) {
    assert(tlArgsIs(args));
    if (!args->names) return null;
    for (int i = 0; i < args->size; i++) {
        if (tlListGet(args->names, i) == name) return args->args[i];
    }
    return null;
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
    assert(at >= 0 && at < args->size);
    args->args[at] = v;
}
void tlArgsMapSet_(tlArgs* args, tlSym name, tlHandle v) {
    assert(tlArgsIs(args));
    fatal("should not use this");
}
static tlHandle _args_size(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlINT(tlArgsSize(as));
}
static tlHandle _args_first(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlMAYBE(tlArgsGet(as, 0));
}
static tlHandle _args_last(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlMAYBE(tlArgsGet(as, tlArgsSize(args) - 1));
}
static tlHandle _args_has(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle v = tlArgsGet(args, 0);
    if (!v || tlIntIs(v) || tlFloatIs(v)) {
        int at = at_offset(tlArgsGet(args, 0), tlArgsSize(as));
        return tlBOOL(tlArgsGet(as, at) != 0);
    }
    if (tlSymIs(v)) {
        return tlBOOL(tlArgsMapGet(as, tlSymAs(v)) != 0);
    }
    TL_THROW("Expected an index or name");
}
static tlHandle _args_get(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle v = tlArgsGet(args, 0);
    if (!v || tlIntIs(v) || tlFloatIs(v)) {
        int at = at_offset(tlArgsGet(args, 0), tlArgsSize(as));
        tlHandle res = tlArgsGet(as, at);
        if (res) return res;
        return tlMAYBE(tlArgsGet(args, 1));
    }
    if (tlSymIs(v)) {
        tlHandle res = tlArgsMapGet(as, tlSymAs(v));
        if (res) return res;
        return tlMAYBE(tlArgsGet(args, 1));
    }
    TL_THROW("Expected an index or name");
}
static tlHandle _args_this(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsTarget(as);
    return tlMAYBE(res);
}
static tlHandle _args_msg(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsMsg(as);
    return tlMAYBE(res);
}
static tlHandle _args_block(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsBlock(as);
    return tlMAYBE(res);
}
static tlHandle _args_names(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsObject(as);
    return tlMAYBE(res);
}
static tlHandle _args_namesmap(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsMap(as);
    return tlMAYBE(res);
}
// TODO optimize
static tlHandle _args_slice(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));

    tlList* list = tlArgsList(as);
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

static size_t argsSize(tlHandle v) {
    return sizeof(tlArgs) + sizeof(tlHandle) * tlArgsAs(v)->size;
}

static void args_init() {
    tlKind _tlArgsKind = {
        .name = "Args",
        .toString = _ArgstoString,
        .size = argsSize,
    };
    INIT_KIND(tlArgsKind);

    _tl_emptyArgs = tlArgsNewNew(0);
    tlArgsKind->klass = tlClassObjectFrom(
            "this", _args_this,
            "msg", _args_msg,
            "block", _args_block,
            "names", _args_names,
            "namesmap", _args_namesmap,
            //"raw", _args_raw,

            //"hash", _args_hash,
            "size", _args_size,
            "has", _args_has,
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
    tlObjectSet_(constructor, s__methods, tlArgsKind->klass);
    tl_register_global("Args", constructor);
}

