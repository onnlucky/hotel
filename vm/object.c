// a object implementation

#include "trace-off.h"

tlKind* tlObjectKind;
static tlSym s_methods;

struct tlObject {
    tlHead head;
    tlSet* keys;
    tlHandle data[];
};

static tlObject* _tl_emptyObject;

tlObject* tlObjectEmpty() { return tlObjectNew(null); }

tlObject* tlObjectNew(tlSet* keys) {
    if (!keys) keys = _tl_set_empty;
    tlObject* object = tlAlloc(tlObjectKind, sizeof(tlObject) + sizeof(tlHandle) * keys->size);
    object->keys = keys;
    assert(tlObjectSize(object) == tlSetSize(keys));
    return object;
}
tlObject* tlObjectToObject_(tlObject* object) {
    assert((object->head.kind & 0x7) == 0);
    object->head.kind = (intptr_t)tlObjectKind; return object;
}
int tlObjectHash(tlObject* object) {
    // if (object->hash) return object->hash;
    unsigned int hash = 0x1;
    for (int i = 0; i < object->keys->size; i++) {
        hash ^= tlHandleHash(tlSetGet(object->keys, i));
        hash ^= tlHandleHash(object->data[i]);
    }
    if (!hash) hash = 1;
    return hash;
}
int tlObjectSize(const tlObject* object) {
    return object->keys->size;
}
tlSet* tlObjectKeys(const tlObject* object) {
    return object->keys;
}
tlList* tlObjectValues(const tlObject* object) {
    tlList* list = tlListNew(tlObjectSize(object));
    for (int i = 0; i < object->keys->size; i++) {
        assert(object->data[i]);
        tlListSet_(list, i, object->data[i]);
    }
    return list;
}
void tlObjectDump(tlObject* object) {
    print("---- MAP DUMP @ %p ----", object);
    for (int i = 0; i < tlObjectSize(object); i++) {
        print("%d %s: %s", i, tl_str(object->keys->data[i]), tl_str(object->data[i]));
    }
    print("----");
}

tlHandle tlObjectGet(tlObject* object, tlHandle key) {
    assert(tlObjectIs(object) || tlMapIs(object));
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof(object->keys, key);
    if (at < 0) return null;
    assert(at < tlObjectSize(object));
    trace("keys get: %s = %s", tl_str(key), tl_str(object->data[at]));
    return object->data[at];
}
tlObject* tlObjectSet(tlObject* object, tlHandle key, tlHandle v) {
    assert(tlObjectIs(object) || tlMapIs(object));
    if (tlStringIs(key)) key = tlSymFromString(key);
    trace("set object: %s = %s", tl_str(key), tl_str(v));

    int at = 0;
    at = tlSetIndexof(object->keys, key);
    if (at >= 0) {
        tlObject* nobject = tlClone(object);
        nobject->data[at] = v;
        return nobject;
    }

    int size = tlSetSize(object->keys) + 1;
    tlSet* keys = tlSetCopy(object->keys, size);
    at = tlSetAdd_(keys, key);

    tlObject* nobject = tlObjectNew(keys);
    int i;
    for (i = 0; i < at; i++) {
        nobject->data[i] = object->data[i];
    }
    nobject->data[at] = v; i++;
    for (; i < size; i++) {
        nobject->data[i] = object->data[i - 1];
    }
    set_kind(nobject, tl_kind(object));
    return nobject;
}

tlHandle tlObjectGetSym(const tlObject* object, tlHandle key) {
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof(object->keys, key);
    if (at < 0) return null;
    assert(at < tlObjectSize(object));
    trace("keys get: %s = %s", tl_str(key), tl_str(object->data[at]));
    return object->data[at];
}
void tlObjectSet_(tlObject* object, tlHandle key, tlHandle v) {
    assert(tlObjectIs(object) || tlMapIs(object));
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof(object->keys, key);
    //print("%s", tl_repr(object));
    //print("keys set_: %d at:%s = %s", at, tl_str(key), tl_str(v));
    assert(at >= 0 && at < tlObjectSize(object));
    //assert(object->data[at] == tlNull || !object->data[at]);
    object->data[at] = v;
}

tlHandle tlObjectValueIter(const tlObject* object, int i) {
    assert(i >= 0);
    if (i >= tlObjectSize(object)) return null;
    return object->data[i];
}
tlHandle tlObjectKeyIter(const tlObject* object, int i) {
    assert(i >= 0);
    if (i >= tlObjectSize(object)) return null;
    return object->keys->data[i];
}
bool tlObjectKeyValueIter(const tlObject* object, int i, tlHandle* keyp, tlHandle* valuep) {
    assert(i >= 0);
    if (i >= tlObjectSize(object)) return false;
    *keyp = object->keys->data[i];
    *valuep = object->data[i];
    return true;
}

void tlObjectValueIterSet_(tlObject* object, int i, tlHandle v) {
    assert(i >= 0 && i < tlObjectSize(object));
    object->data[i] = v;
}

tlObject* tlObjectFromPairs(tlList* pairs) {
    int size = tlListSize(pairs);
    trace("%d", size);

    tlSet* keys = tlSetNew(size);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlSymAs(tlListGet(pair, 0));
        tlSetAdd_(keys, name);
    }
    int realsize = size;
    for (int i = 0; i < size; i++) {
        if (tlSetGet(keys, size - 1 - i)) break;
        realsize--;
    }
    if (realsize != size) keys->size = realsize;

    tlObject* o = tlObjectNew(keys);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlSymAs(tlListGet(pair, 0));
        tlHandle v = tlListGet(pair, 1);
        tlObjectSet_(o, name, v);
    }
    return o;
}

tlObject* tlObjectFrom(const char* n1, tlHandle v1, ...) {
    va_list ap;
    int size = 1;
    assert(n1);
    assert(v1);

    va_start(ap, v1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlHandle v = va_arg(ap, tlHandle); assert(v);
        size++;
    }
    va_end(ap);

    tlSet* keys = tlSetNew(size);
    tlSetAdd_(keys, tlSYM(n1));
    va_start(ap, v1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlHandle);
        tlSetAdd_(keys, tlSYM(n));
    }
    va_end(ap);

    tlObject* object = tlObjectNew(keys);
    tlObjectSet_(object, tlSYM(n1), v1);
    va_start(ap, v1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlHandle v = va_arg(ap, tlHandle); assert(v);
        tlObjectSet_(object, tlSYM(n), v);
    }
    va_end(ap);

    return tlObjectToObject_(object);
}

tlObject* tlClassObjectFrom(const char* n1, tlNativeCb fn1, ...) {
    va_list ap;
    int size = 1;

    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlNativeCb);
        size++;
    }
    va_end(ap);

    tlSet* keys = tlSetNew(size);
    tlSetAdd_(keys, tlSYM(n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlNativeCb);
        tlSetAdd_(keys, tlSYM(n));
    }
    va_end(ap);

    tlObject* object = tlObjectNew(keys);
    if (fn1) tlObjectSet_(object, tlSYM(n1), tlNATIVE(fn1, n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlNativeCb fn = va_arg(ap, tlNativeCb);
        if (fn) tlObjectSet_(object, tlSYM(n), tlNATIVE(fn, n));
    }
    va_end(ap);

    return tlObjectToObject_(object);
}

// called when map literals contain lookups or expressions to evaluate
static tlHandle _Object_clone(tlTask* task, tlArgs* args) {
    tlObject* o = tlObjectCast(tlArgsGet(args, 0));
    if (!o) TL_THROW("Expected an Object");
    int size = tlObjectSize(o);
    o = tlClone(o);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!o->data[i] || tlUndefinedIs(o->data[i])) {
            o->data[i] = tlArgsGet(args, argc++);
        }
    }
    assert(argc == tlArgsSize(args));
    return o;
}

tlObject* tlObjectFromMap(tlMap* map);
tlObject* tlHashMapToObject(tlHashMap*);
static tlHandle _Object_from(tlTask* task, tlArgs* args) {
    tlHandle a1 = tlArgsGet(args, 0);
    if (tlHashMapIs(a1)) return tlHashMapToObject(a1);
    if (tlMapIs(a1)) return tlObjectFromMap(a1);
    if (tlObjectIs(a1)) return a1;
    TL_THROW("expect a Map, Object, or HashMap");
}

// {class=SomeThing},{foo=bar,class=brr} -> {class=SomeThing,foo=bar}
static tlHandle _Object_inherit(tlTask* task, tlArgs* args) {
    tlObject* o = tlObjectCast(tlArgsGet(args, 0));
    if (!o) TL_THROW("Expected an Object");
    tlHandle oclass = tlObjectGetSym(o, s_class);
    for (int i = 1; i < tlArgsSize(args); i++) {
        if (!tlObjectIs(tlArgsGet(args, i))) TL_THROW("Expected an Object");
        tlObject* add = tlArgsGet(args, i);
        for (int i = 0; i < add->keys->size; i++) {
            o = tlObjectSet(o, add->keys->data[i], add->data[i]);
        }
    }
    if (oclass) o = tlObjectSet(o, s_class, oclass);
    return o;
}
static tlHandle _Object_hash(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlINT(tlObjectHash(object));
}
static tlHandle _Object_size(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlINT(tlObjectSize(object));
}
static tlHandle _Object_keys(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlObjectKeys(object);
}
static tlHandle _Object_values(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlObjectValues(object);
}
static tlHandle _Object_toMap(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlMapFromObject(object);
}
static tlHandle _Object_has(tlTask* task, tlArgs* args) {
    trace("");
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a String");
    tlHandle res = tlObjectGetSym(object, tlSymFromString(key));
    return tlBOOL(res != null);
}
static tlHandle _Object_get(tlTask* task, tlArgs* args) {
    trace("");
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a String");
    tlHandle res = tlObjectGetSym(object, tlSymFromString(key));
    return tlMAYBE(res);
}
static tlHandle _Object_set(tlTask* task, tlArgs* args) {
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlArgsGet(args, 0);
    if (tlObjectIs(tlArgsGet(args, 1))) {
        tlObject* from = tlArgsGet(args, 1);
        for (int i = 0; i < 1000; i++) {
            tlHandle key = tlObjectKeyIter(from, i);
            if (!key) return tlObjectToObject_(object);
            tlHandle val = tlObjectValueIter(from, i);
            assert(val);
            object = tlObjectSet(object, key, val);
        }
    }
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a String");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("Expected a value");
    return tlObjectToObject_(tlObjectSet(object, tlSymFromString(key), val));
}

static tlHandle _Object_merge(tlTask* task, tlArgs* args) {
    tlHashMap* res = tlHashMapNew();
    for (int i = 0;; i++) {
        tlHandle arg = tlArgsGet(args, i);
        if (!arg) break;
        tlObject* o = tlObjectCast(arg);
        if (o) {
            for (int j = 0;; j++) {
                tlHandle k = tlObjectKeyIter(o, j);
                if (!k) break;
                tlHandle v = tlObjectValueIter(o, j);
                tlHashMapSet(res, k, v);
            }
            continue;
        }
        tlMap* map = tlMapCast(arg);
        if (map) {
            for (int j = 0;; j++) {
                tlHandle k = tlMapKeyIter(map, j);
                if (!k) break;
                tlHandle v = tlMapValueIter(map, j);
                tlHashMapSet(res, k, v);
            }
            continue;
        }
        //tlHashMap* hash = tlHashMapCast(arg);
        TL_THROW("Expected a Object or Map as arg[%d]", i);
    }
    return tlHashMapToObject(res);
}

static size_t objectSize(tlHandle v) {
    return sizeof(tlObject) + sizeof(tlHandle) * tlObjectAs(v)->keys->size;
}
const char* objecttoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Object@%p %d>", v, tlObjectSize(tlObjectAs(v))); return buf;
}

INTERNAL tlHandle objectResolve(tlObject* object, tlSym msg) {
    tlHandle field = null;
    tlObject* cls;

    // search for field
    cls = object;
    while (cls && tlObjectIs(cls)) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(msg));
        field = tlObjectGet(cls, msg);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

    // search for getter
    cls = object;
    while (cls && tlObjectIs(cls)) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(s__get));
        field = tlObjectGet(cls, s__get);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

send:;
    trace("MAP RESOLVE %p: %s -> %s", object, tl_str(msg), tl_str(field));
    return field;
}

// TODO remove? object resolve is always used ...
static tlHandle objectSend(tlTask* task, tlArgs* args, bool safe) {
    tlObject* object = tlObjectAs(tlArgsTarget(args));
    tlSym msg = tlArgsMsg(args);
    tlHandle field = null;
    tlObject* cls;

    // search for field
    cls = object;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(msg));
        field = tlObjectGet(cls, msg);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

    // search for getter
    cls = object;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(s__get));
        field = tlObjectGet(cls, s__get);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

send:;
    trace("VALUE SEND %p: %s -> %s", object, tl_str(msg), tl_str(field));
    if (!field) {
        if (safe) return tlNull;
        TL_THROW("%s.%s is undefined", tl_str(object), tl_str(msg));
    }
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(task, args, field);
}

static tlHandle objectRun(tlTask* task, tlHandle fn, tlArgs* args) {
    assert(tlTaskIs(task));
    tlObject* object = tlObjectAs(fn);
    tlHandle field = tlObjectGetSym(object, s_call);
    if (field) {
        trace("%s", tl_str(field));
        // TODO this is awkward, tlCallableIs does not know about complex user objects
        if (!tlCallableIs(field)) return field;
        return tlEvalArgsTarget(task, args, fn, field);
    }
    TL_THROW("'%s' not callable", tl_str(fn));
}

static unsigned int objectHash(tlHandle v) {
    tlObject* object = tlObjectAs(v);
    return tlObjectHash(object);
}
static bool objectEquals(tlHandle _left, tlHandle _right) {
    if (_left == _right) return true;

    tlObject* left = tlObjectAs(_left);
    tlObject* right = tlObjectAs(_right);
    if (left->keys->size != right->keys->size) return false;

    for (int i = 0; i < left->keys->size; i++) {
        if (!tlHandleEquals(tlSetGet(left->keys, i), tlSetGet(right->keys, i))) return false;
        if (!tlHandleEquals(left->data[i], right->data[i])) return false;
    }
    return true;
}
static tlHandle objectCmp(tlHandle _left, tlHandle _right) {
    if (_left == _right) return tlEqual;

    tlObject* left = tlObjectAs(_left);
    tlObject* right = tlObjectAs(_right);
    int size = MIN(left->keys->size, right->keys->size);
    for (int i = 0; i < size; i++) {
        tlHandle cmp = tlHandleCompare(tlSetGet(left->keys, i), tlSetGet(right->keys, i));
        if (cmp != tlEqual) return cmp;
        cmp = tlHandleCompare(left->data[i], right->data[i]);
        if (cmp != tlEqual) return cmp;
    }
    return tlCOMPARE(left->keys->size - right->keys->size);
}

static tlHandle _bless(tlTask* task, tlArgs* args) {
    tlObject* methods = tlObjectCast(tlArgsGet(args, 0));
    if (!methods) TL_THROW("expected an object as arg[1]");
    tlHandle klass = tlArgsGet(args, 1);
    if (!klass) TL_THROW("expected a object as arg[2]");
    trace("methods: %s", tl_repr(methods));
    trace("class: %s", tl_repr(klass));
    tlObjectSet_(methods, s_class, klass);
    tlObjectSet_(klass, s_methods, methods);
    return klass;
}

static tlKind _tlObjectKind = {
    .name = "Object",
    .size = objectSize,
    .hash = objectHash,
    .equals = objectEquals,
    .cmp = objectCmp,
    .toString = objecttoString,
    .send = objectSend,
    .run = objectRun,
};

static void object_init() {
    s_methods = tlSYM("methods");
    _tl_emptyObject = tlObjectNew(tlSetEmpty());
    tlObject* constructor = tlClassObjectFrom(
        "call", _Object_from,
        "hash", _Object_hash,
        "size", _Object_size,
        "has", _Object_has,
        "get", _Object_get,
        "set", _Object_set,
        "keys", _Object_keys,
        "values", _Object_values,
        "toMap", _Object_toMap,
        "merge", _Object_merge,
        "inherit", _Object_inherit,
        "each", null,
        "map", null,
        null
    );
    tl_register_global("Object", constructor);
}

