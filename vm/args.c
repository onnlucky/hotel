// author: Onne Gorter, license: MIT (see license.txt)

// args, an argument object passed to all functions native and user level

// TODO the names list, can have a set as last element, to make args.object and args.map faster

#include "platform.h"
#include "args.h"

#include "value.h"
#include "object.h"
#include "map.h"

tlKind* tlArgsKind;

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

// create a new args, changing/adding function, method, target
tlArgs* tlArgsFrom(tlArgs* call, tlHandle fn, tlSym method, tlHandle target) {
    uint32_t size = call->size - (call->spec & 3);
    tlList* names = tlArgsNamesInline(call);
    tlArgs* args = tlArgsNewNames(size, !!names, method || target);
    int call_offset = call->spec & 3;
    int args_offset = args->spec & 3;
    for (int i = 0; i < size; i++) {
        args->data[args_offset + i] = call->data[call_offset + i];
    }
    tlArgsSetFn_(args, fn);
    if (names) {
        tlArgsSetNames_(args, names, 0);
    }
    if (method || target) {
        tlArgsSetMethod_(args, method);
        tlArgsSetTarget_(args, target);
    }
    return args;
}

// create a new args, changing/adding function, method, target, add prepending 1 arg in front
tlArgs* tlArgsFromPrepend1(tlArgs* call, tlHandle fn, tlSym method, tlHandle target, tlHandle arg1) {
    uint32_t size = call->size + 1 - (call->spec & 3);
    tlList* names = tlArgsNamesInline(call);
    tlArgs* args = tlArgsNewNames(size, !!names, method || target);
    int call_offset = call->spec & 3;
    int args_offset = args->spec & 3;
    assert((args->spec & 3) == 2);

    args->data[args_offset] = arg1;
    for (int i = 0; i < size; i++) {
        args->data[args_offset + i + 1] = call->data[call_offset + i];
    }
    tlArgsSetFn_(args, fn);
    if (names) {
        tlArgsSetNames_(args, tlListPrepend(names, null), 0);
    }
    if (method || target) {
        tlArgsSetMethod_(args, method);
        tlArgsSetTarget_(args, target);
    }
    return args;
}


// for debugging
void tlArgsDump(tlArgs* args) {
    print("--- args dump: %d named=%d", tlArgsSize(args), tlArgsNamedSize(args));
    for (int i = 0;; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) return;
        print("args: %s", tl_repr(v));
    }
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

void tlArgsMakeSetter_(tlArgs* args) {
    tlflag_set(args, kIsSetter);
}

bool tlArgsIsSetter(tlArgs* args) {
    return tlflag_isset(args, kIsSetter);
}

tlHandle tlArgsFn(tlArgs* args) {
    return args->fn;
}

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
    assert(size == 0 || tlListGet(list, size - 1));
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
    if (args->spec == 0b101 && args->data[0] == TL_NAMED_NULL_BLOCK) return args->data[2];
    if (args->spec == 0b111 && args->data[2] == TL_NAMED_NULL_BLOCK) return args->data[4];
    if ((args->spec & 1) == 0) return null;
    assert(tlArgsNames(args) != TL_NAMED_NULL_BLOCK);
    return tlArgsGetNamed(args, s_block);
}

tlHandle tlArgsLhs(tlArgs* args) {
    if (args->spec == 0b1011 && args->data[2] == TL_NAMED_NULL_LHS_RHS) return args->data[4];
    if ((args->spec & 1) == 0) return null;
    assert(tlArgsNames(args) != TL_NAMED_NULL_LHS_RHS);
    return tlArgsGetNamed(args, s_lhs);
}

tlHandle tlArgsRhs(tlArgs* args) {
    if (args->spec == 0b1011 && args->data[2] == TL_NAMED_NULL_LHS_RHS) return args->data[5];
    if ((args->spec & 1) == 0) return null;
    assert(tlArgsNames(args) != TL_NAMED_NULL_LHS_RHS);
    return tlArgsGetNamed(args, s_rhs);
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

//. object Args

//. size: returns how many arguments were passed in, count does not inlude named params
static tlHandle _args_size(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlINT(tlArgsSize(as));
}

//. first: returns the first argument, does not include named params
static tlHandle _args_first(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlOR_UNDEF(tlArgsGet(as, 0));
}

//. last: returns the last argument, does not include named params
static tlHandle _args_last(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    return tlOR_UNDEF(tlArgsGet(as, tlArgsSize(args) - 1));
}

//. has[at|name]: returns true if there is an argument at position #at or with name #name
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

//. get(at|name): returns the argument at position #at or with name #name
static tlHandle _args_get(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle v = tlArgsGet(args, 0);
    if (!v || tlIntIs(v) || tlFloatIs(v)) {
        int at = at_offset(tlArgsGet(args, 0), tlArgsSize(as));
        tlHandle res = tlArgsGet(as, at);
        if (res) return res;
        return tlOR_UNDEF(tlArgsGet(args, 1));
    }
    if (tlSymIs(v)) {
        tlHandle res = tlArgsGetNamed(as, tlSymAs(v));
        if (res) return res;
        return tlOR_UNDEF(tlArgsGet(args, 1));
    }
    TL_THROW("Expected an index or name");
}

//. this: returns the target of these arguments
static tlHandle _args_this(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsTarget(as);
    return tlOR_UNDEF(res);
}

//. msg: returns the message (the method) name of these arguments
static tlHandle _args_msg(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsMethod(as);
    return tlOR_UNDEF(res);
}

//. block: returns the block passed into these arguments, will not invoke the block, equivalent to
//. `args["block"]` or `args.get("block")`
static tlHandle _args_block(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsBlock(as);
    return tlOR_UNDEF(res);
}

//. lhs: returns the lhs, useful for the args to an operator method
//. `args["lhs"]`
static tlHandle _args_lhs(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsLhs(as);
    return tlOR_UNDEF(res);
}

//. rhs: returns the rhs, useful for the args to an operator method
//. `args["rhs"]`
static tlHandle _args_rhs(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsRhs(as);
    return tlOR_UNDEF(res);
}

//. names: return all the named arguments as an #Object
static tlHandle _args_names(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsObject(as);
    return tlOR_UNDEF(res);
}

//. namesmap: return all the named arguments as a #Map
static tlHandle _args_namesmap(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));
    tlHandle res = tlArgsMap(as);
    return tlOR_UNDEF(res);
}

//. slice(begin, end): return a range of the unnamed arguments in a list, equivalent to
//. `args.toList[begin:end]`
static tlHandle _args_slice(tlTask* task, tlArgs* args) {
    tlArgs* as = tlArgsAs(tlArgsTarget(args));

    tlList* list = tlArgsList(as);
    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), -1);
    int offset;
    int len = sub_offset(first, last, tlListSize(list), &offset);
    return tlListSub(list, offset, len);
}

//. Args(): returns an new arguments object representing what you passed in
static tlHandle _Args_call(tlTask* task, tlArgs* args) {
    return args;
}

//. Args.from(list, object): returns an new arguments object, created from the list and object you
//. passed in
// TODO object or map, list or array, etc...
static tlHandle _Args_from(tlTask* task, tlArgs* args) {
    tlHandle a1 = tlArgsGet(args, 0);
    tlHandle a2 = tlArgsGet(args, 1);
    if (a1 && !tlListIs(a1)) TL_THROW("args[1] must be a list, not '%s'", tl_str(a1));
    if (a2 && !tlObjectIs(a2)) TL_THROW("args[2] must be an object, not '%s'", tl_str(a2));

    tlList* list = tlListAs(a1);
    int lsize = list? tlListSize(list) : 0;
    tlObject* obj = tlObjectAs(a2);
    int osize = obj? tlObjectSize(a2) : 0;
    bool hasNames = osize > 0;
    tlList* named = hasNames? tlListNew(lsize + osize) : null;

    tlArgs* call = tlArgsNewNames(lsize + osize, hasNames, false);
    for (int i = 0; i < lsize; i++) {
        tlArgsSet_(call, i, tlListGet(list, i));
        if (hasNames) tlListSet_(named, i, tlNull);
    }
    for (int i = 0; i < osize; i++) {
        tlSym key;
        tlHandle value;
        tlObjectKeyValueIter(obj, i, &key, &value);
        tlListSet_(named, lsize + i, key);
        tlArgsSet_(call, lsize + i, value);
    }
    if (hasNames) tlArgsSetNames_(call, named, osize);
    return call;
}

//. args.invoke(fn): invoke the function with these arguments
//. these two are the same: `print(1, 2)` and `Args(1, 2).invoke(print)`
static tlHandle _args_invoke(tlTask* task, tlArgs* args) {
    TL_TARGET(tlArgs, call);
    tlHandle fn = tlArgsGet(args, 0);
    if (!fn) TL_THROW("invoke require a function to call");

    tlArgs* call2 = tlArgsFrom(call, fn, null, null);
    return tlInvoke(task, call2);
}

const char* _ArgstoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Args@%p %d %d>", v, tlArgsSize(tlArgsAs(v)), tlArgsNamedSize(tlArgsAs(v)));
    return buf;
}

static size_t argsSize(tlHandle v) {
    return sizeof(tlArgs) + sizeof(tlHandle) * tlArgsAs(v)->size;
}

tlList* TL_NAMED_NULL_BLOCK;
tlList* TL_NAMED_NULL_LHS_RHS;

void args_init() {
    TL_NAMED_NULL_BLOCK = tlListFrom2(tlNull, s_block);
    TL_NAMED_NULL_LHS_RHS = tlListFrom(tlNull, s_lhs, s_rhs, null);

    tlClass* cls = tlCLASS("Args", tlNATIVE(_Args_call, "Args"),
    tlMETHODS(
        "this", _args_this,
        "msg", _args_msg,
        "block", _args_block,
        "lhs", _args_lhs,
        "rhs", _args_rhs,
        "names", _args_names,
        "toObject", _args_names,
        "toList", _args_slice,
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

        "invoke", _args_invoke,
        null
    ), tlMETHODS(
        "from", _Args_from,
        null
    ));
    tlKind _tlArgsKind = {
        .name = "Args",
        .toString = _ArgstoString,
        .size = argsSize,
        .cls = cls,
    };
    INIT_KIND(tlArgsKind);
    tl_register_global("Args", cls);

    _tl_emptyArgs = tlArgsNew(0);
}

