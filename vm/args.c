// author: Onne Gorter, license: MIT (see license.txt)
//
// args, an argument object passed to all functions native and user level

// TODO the names list, can have a set as last element, to make args.object and args.map faster

#include "trace-off.h"

tlKind* tlArgsKind;

struct tlArgs {
    tlHead head;
    uint32_t size; // total data size
    uint32_t spec; // 0b...11 count of target/method/names, spec >> 2 = count of names
    tlHandle fn;
    tlHandle data[];
    // first 3 items in data[] are:
    // target; // if spec & 2
    // method; // if spec & 2 (a tlSym)
    // names;  // if spec & 1 (a tlList)
};

static tlArgs* _tl_emptyArgs;
tlArgs* tlArgsEmpty() { return _tl_emptyArgs; }

tlArgs* tlArgsNew(int size) {
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * size);
    args->size = size;
    return args;
}

tlArgs* tlArgsNewNames(int size, bool hasNames, bool isMethod) {
    if (isMethod) size += 2;
    if (hasNames) size += 1;
    tlArgs* args = tlAlloc(tlArgsKind, sizeof(tlArgs) + sizeof(tlHandle) * size);
    args->size = size;
    args->spec = !!isMethod << 1 | !!hasNames;
    return args;
}

int tlArgsSize(tlArgs* args) {
    return args->size - (args->spec >> 2) - (args->spec & 3);
}

int tlArgsNamedSize(tlArgs* args) {
    return args->spec >> 2;
}

int tlArgsRawSize(tlArgs* args) {
    return args->size - (args->spec & 3);
}

tlHandle tlArgsFn(tlArgs* args) { return args->fn; }

tlHandle tlArgsTarget(tlArgs* args) {
    if (args->spec & 2) return args->data[0];
    return null;
}

tlHandle tlArgsMethod(tlArgs* args) {
    if (args->spec & 2) return args->data[1];
    return null;
}

tlList* tlArgsNames(tlArgs* args) {
    if (args->spec & 1) return args->data[args->spec & 2];
    return null;
}

tlList* tlArgsList(tlArgs* args) {
    assert(tlArgsIs(args));
    int size = tlArgsSize(args);
    tlList* list = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlListSet_(list, i, tlArgsGet(args, i));
    }
    assert(tlListGet(list, size - 1));
    return list;
}

tlObject* tlArgsObject(tlArgs* args) {
    assert(tlArgsIs(args));
    tlList* names = tlArgsNames(args);
    if (!names) return tlObjectEmpty();

    assert(tlListSize(names) == tlArgsRawSize(args));
    int nsize = tlArgsNamedSize(args);
    int size = tlArgsRawSize(args);
    tlSet* set = tlSetNew(nsize);
    for (int i = 0; i < size; i++) {
        tlHandle name = tlListGet(names, i);
        if (!tlSymIs(name)) continue;
        tlSetAdd_(set, tlSymAs(name));
    }

    tlObject* object = tlObjectNew(set);
    for (int i = 0; i < size; i++) {
        tlHandle name = tlListGet(names, i);
        if (!tlSymIs(name)) continue;
        tlObjectSet_(object, tlSymAs(name), args->data[(args->spec & 3) + i]);
    }
    assert(tlObjectSize(object) == tlArgsNamedSize(args));
    return object;
}

tlMap* tlArgsMap(tlArgs* args) {
    return tlMapFromObject_(tlArgsObject(args));
}

tlHandle tlArgsGetRaw(tlArgs* args, int at) {
    if (at < 0 || at >= tlArgsRawSize(args)) return null;
    return args->data[(args->spec & 3) + at];
}

// this is a linear scan, if there are names set
tlHandle tlArgsGet(tlArgs* args, int at) {
    if (at < 0 || at >= tlArgsSize(args)) return null;
    tlList* names = tlArgsNames(args);
    if (!names) return args->data[(args->spec & 3) + at];

    int target = -1;
    int size = tlArgsRawSize(args);
    for (int i = 0; i < size; i++) {
        if (!tlSymIs(tlListGet(names, i))) target++;
        if (target == at) return args->data[(args->spec & 3) + i];
    }
    fatal("precondition failed");
    return null;
}

// also a linear scan
tlHandle tlArgsGetNamed(tlArgs* args, tlSym name) {
    assert(tlSymIs_(name));
    assert(tlArgsIs(args));
    tlList* names = tlArgsNames(args);
    trace("%s %s", tl_repr(name), tl_repr(names));
    if (!names) return null;
    int size = tlListSize(names);
    for (int i = 0; i < size; i++) {
        if (name == tlListGet(names, i)) return args->data[(args->spec & 3) + i];
    }
    return null;
}

tlHandle tlArgsBlock(tlArgs* args) {
    return tlArgsGetNamed(args, s_block);
}

void tlArgsSetNames_(tlArgs* args, tlList* names, int nsize) {
    assert(args->spec & 1);
    assert(tlListSize(names) == tlArgsRawSize(args));
    if (nsize == 0) {
        int size = tlListSize(names);
        for (int i = 0; i < size; i++) {
            assert(tlListGet(names, i));
            if (tlSymIs(tlListGet(names, i))) nsize++;
        }
    }
    assert(nsize <= tlArgsRawSize(args));

    args->spec = nsize << 2 | (args->spec & 3);
    args->data[args->spec & 2] = names;
}

void tlArgsSetFn_(tlArgs* args, tlHandle fn) {
    assert(tlArgsIs(args));
    assert(!args->fn);
    args->fn = fn;
}

void tlArgsSetTarget_(tlArgs* args, tlHandle target) {
    assert(tlArgsIs(args));
    assert(args->spec & 2);
    assert(!args->data[0]);
    args->data[0] = target;
}

void tlArgsSetMethod_(tlArgs* args, tlSym method) {
    assert(tlArgsIs(args));
    assert(args->spec & 2);
    assert(!args->data[1]);
    args->data[1] = method;
}

// unlinke the getters, this does not take names into account
void tlArgsSet_(tlArgs* args, int at, tlHandle v) {
    assert(tlArgsIs(args));
    assert(at >= 0 && at < tlArgsRawSize(args));
    assert(!args->data[(args->spec & 3) + at]);
    args->data[(args->spec & 3) + at] = v;
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
        return tlBOOL(tlArgsGetNamed(as, tlSymAs(v)) != 0);
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
        tlHandle res = tlArgsGetNamed(as, tlSymAs(v));
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
    tlHandle res = tlArgsMethod(as);
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

static tlHandle _Args_from(tlTask* task, tlArgs* args) {
    return args;
}

const char* _ArgstoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Args@%p %d %d>", v, tlArgsSize(tlArgsAs(v)), tlArgsNamedSize(tlArgsAs(v)));
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

    _tl_emptyArgs = tlArgsNew(0);
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
        "call", _Args_from,
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s__methods, tlArgsKind->klass);
    tl_register_global("Args", constructor);
}

