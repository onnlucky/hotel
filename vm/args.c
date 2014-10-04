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

static tlArgs* _tl_emptyArgs;
tlArgs* tlArgsEmpty() { return _tl_emptyArgs; }

tlArgs* tlArgsNewNames(int size, tlSet* names) {
    warning("shouldn't use this");
    int nsize = tlSetSize(names);
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * (size + nsize));
    args->size = size;
    args->nsize = nsize;
    //if (!names) names = _tl_set_empty;
    //args->list = tlListNew(size - tlSetSize(names));
    //args->map = tlObjectNew(names);
    return args;
}

tlArgs* tlArgsNewSize(int size, int nsize) {
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * (size + nsize));
    args->size = size;
    args->nsize = nsize;
    return args;
}

tlArgs* tlArgsNew(tlList* list, tlObject* map) {
    //warning("shouldn't use this");
    if (!list) list = _tl_emptyList;
    if (!map) map = tlObjectEmpty();
    int size = tlListSize(list);
    int nsize = tlObjectSize(map);
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * (size + nsize));
    args->size = size;
    args->nsize = nsize;
    for (int i = 0; i < size; i++) {
        args->args[i] = tlListGet(list, i);
    }
    if (nsize > 0) {
        args->names = tlListNew(size + nsize);
        for (int i = 0; i < size; i++) tlListSet_(args->names, i, tlNull);
        for (int i = 0; i < nsize; i++) {
            tlHandle name;
            tlHandle value;
            tlObjectKeyValueIter(map, i, &name, &value);
            tlListSet_(args->names, size + i, name);
            args->args[size +i] = value;
        }
    }
    return args;
}

tlHandle tlArgsTarget(tlArgs* args) { assert(tlArgsIs(args)); return args->target; }
tlHandle tlArgsMsg(tlArgs* args) { assert(tlArgsIs(args)); return args->msg; }
tlHandle tlArgsFn(tlArgs* args) { assert(tlArgsIs(args)); return args->fn; }

int tlArgsSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->size;
}
int tlArgsMapSize(tlArgs* args) {
    assert(tlArgsIs(args));
    return args->nsize;
}
tlList* tlArgsList(tlArgs* args) {
    warning("shouldn't use this");
    assert(tlArgsIs(args));
    tlList* list = tlListNew(args->size);
    for (int i = 0; i < args->size; i++) tlListSet_(list, i, args->args[i]);
    return list;
}
tlObject* tlArgsObject(tlArgs* args) {
    warning("shouldn't use this");
    assert(tlArgsIs(args));
    int offset = args->size;
    tlSet* set = tlSetNew(args->nsize);
    for (int i = 0; i < args->nsize; i++) tlSetAdd_(set, tlListGet(args->names, offset + i));

    tlObject* object = tlObjectNew(set);
    for (int i = 0; i < args->nsize; i++) tlObjectSet_(object, tlListGet(args->names, offset + i), args->args[offset + i]);
    return object;
}
tlMap* tlArgsMap(tlArgs* args) {
    assert(tlArgsIs(args));
    return tlMapFromObject_(tlArgsObject(args));
}
tlHandle tlArgsGet(const tlArgs* args, int at) {
    if (at < 0 || at >= args->size) return null;
    return args->args[at];
}
tlHandle tlArgsMapGet(tlArgs* args, tlSym name) {
    assert(tlArgsIs(args));
    int offset = args->size;
    for (int i = 0; i < args->nsize; i++) {
        if (tlListGet(args->names, offset + i) == name) return args->args[offset + i];
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
    fatal("not implemented");
    //tlObjectSet_(args->map, name, v);
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
static tlHandle _args_has(tlArgs* args) {
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
static tlHandle _args_get(tlArgs* args) {
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
// TODO optimize
static tlHandle _args_slice(tlArgs* args) {
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


static void args_init() {
    tlKind _tlArgsKind = {
        .name = "Args",
        .toString = _ArgstoString,
    };
    INIT_KIND(tlArgsKind);

    _tl_emptyArgs = tlArgsNew(null, null);
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

