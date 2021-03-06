// author: Onne Gorter, license: MIT (see license.txt)

// a object implementation

#include "platform.h"
#include "object.h"

#include "value.h"
#include "set.h"
#include "args.h"
#include "string.h"
#include "list.h"
#include "bcode.h"

tlKind* tlClassKind;

static tlClass* _tl_class;

tlClass* tlCLASS(const char* name, tlNative* ctor, tlObject* methods, tlObject* statics) {
    tlClass* cls = tlAlloc(tlClassKind, sizeof(tlClass));
    cls->name = tlSYM(name);
    cls->constructor = ctor;
    cls->methods = methods? methods : tlObjectEmpty();
    cls->statics = statics? statics : tlObjectEmpty();
    return cls;
}

tlClass* tlClassFor(tlHandle value) {
    if (!value) return null;
    return tl_kind(value)->cls;
}

static tlHandle _Class_name(tlTask* task, tlArgs* args) {
    tlClass* cls = tlClassAs(tlArgsTarget(args));
    return cls->name;
}
static tlHandle _Class_class(tlTask* task, tlArgs* args) {
    assert(tlClassIs(tlArgsTarget(args)));
    return _tl_class;
}

const char* classtoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Class@%p %s>", v, tl_str(tlClassAs(v)->name)); return buf;
}

tlKind _tlClassKind = {
    .name = "Class",
    // .hash = classHash,
    // .equals = classEquals,
    // .cmp = classCmp,
    .toString = classtoString,
};

void class_init() {
    _tl_class = tlCLASS("Class",
    null,
    tlMETHODS(
        "name", _Class_name,
        "class", _Class_class,
        null
    ),
    tlMETHODS(
        null, null
    ));
    tlClassKind->cls = _tl_class;
    tl_register_global("Class", _tl_class);
}

tlHandle classResolveStatic(tlClass* cls, tlSym name) {
    if (name == s_class) return _tl_class;
    return tlObjectGet(cls->statics, name);
}

// resolve a name to a method, walking up the super hierarchy as needed; mixins go before methods
tlHandle classResolve(tlClass* cls, tlSym name) {
    assert(cls);
    assert(tl_kind(cls));
    if (name == s_class) return cls;

    tlClass* super = cls;
    while (super) {
        if (super->mixins) {
            for (int i = 0, l = tlListSize(super->mixins); i < l; i++) {
                tlHandle m = tlListGet(super->mixins, i);
                if (tlClassIs(m)) {
                    tlHandle v = classResolve(m, name);
                    if (v) return v;
                } else if (tlObjectIs(m)) {
                    tlHandle v = tlObjectGet(m, name);
                    if (v) return v;
                } else {
                    assert(false);
                }
            }
        }
        tlHandle v = tlObjectGet(super->methods, name);
        if (v) return v;
        super = super->super;
    }
    return null;
}

tlKind* tlObjectKind;

static tlObject* _tl_emptyObject;

tlObject* tlObjectEmpty() {
    assert(_tl_emptyObject);
    assert(tlObjectIs(_tl_emptyObject));
    //return _tl_emptyObject;
    // TODO remove this, but we sometimes transform an fresh object into a Map
    return tlObjectNew(tlSetEmpty());
}

tlObject* tlObjectNew(tlSet* keys) {
    assert(keys);
    tlObject* object = tlAlloc(tlObjectKind, sizeof(tlObject) + sizeof(tlHandle) * tlSetSize(keys));
    object->keys = keys;
    assert(tlObjectSize(object) == tlSetSize(keys));
    return object;
}
tlObject* tlObjectToObject_(tlObject* object) {
    assert((object->head.kind & 0x7) == 0);
    object->head.kind = (intptr_t)tlObjectKind; return object;
}

uint32_t tlObjectHash(tlObject* object, tlHandle* unhashable) {
    // if (object->hash) return object->hash;
    uint32_t hash = 212601863; // 11.hash + 1
    uint32_t size = object->keys->size;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t k = tlHandleHash(tlSetGet(object->keys, i), unhashable);
        uint32_t v = tlHandleHash(object->data[i], unhashable);
        if (!k || !v) return 0;
        // rotate shift the hash then mix in the key, rotate, then value
        hash = hash << 1 | hash >> 31;
        hash ^= k;
        hash = hash << 1 | hash >> 31;
        hash ^= v;
    }
    // and mix in the list size after a murmur
    hash ^= murmurhash2a(&size, sizeof(size));
    return hash | !hash;
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

tlObject* tlObjectDel(tlObject* object, tlSym key) {
    int at = -1;
    assert(tlSetIs(object->keys));
    tlSet* nkeys = tlSetDel(object->keys, key, &at);
    if (at < 0) return object;

    int size = tlObjectSize(object);

    tlObject* nobject = tlObjectNew(nkeys);
    int i = 0;
    for (; i < at; i++) nobject->data[i] = object->data[i];
    i++;
    for (; i < size; i++) nobject->data[i - 1] = object->data[i];
    assert(tlSetIs(object->keys));
    return nobject;
}

// merge two objects recursively, o2 takes precedence
tlObject* tlObjectMerge(tlObject* o1, tlObject* o2) {
    assert(tlObjectIs(o1));
    assert(tlObjectIs(o2));

    tlSet* nkeys = tlSetUnion(o1->keys, o2->keys);
    assert(tlSetIs(nkeys));
    int size = tlSetSize(nkeys);
    tlObject* object = tlObjectNew(nkeys);
    for (int i = 0; i < size; i++) {
        tlSym k = tlSetGet(nkeys, i);
        tlHandle v1 = tlObjectGet(o1, k);
        tlHandle v2 = tlObjectGet(o2, k);
        if (tlObjectIs(v1) && tlObjectIs(v2)) {
            v2 = tlObjectMerge(v1, v2);
        }
        object->data[i] = v2? v2 : v1;
    }
    assert(tlObjectIs(object));
    return object;
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
        tlHandle v = va_arg(ap, tlHandle);
        assert(v);
        UNUSED(v);
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
    if (!n1) {
        assert(!fn1);
        return tlObjectEmpty();
    }
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

//. object Object: contains some utility methods to deal with value objects

tlObject* tlObjectFromMap(tlMap* map);
tlObject* tlHashMapToObject(tlHashMap*);

//. call(in): turn #in into an object
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

//. hash(object): return the hash of the object
static tlHandle _Object_hash(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    tlHandle unhashable = null;
    uint32_t hash = tlObjectHash(object, &unhashable);
    if (!hash) TL_THROW("cannot hash '%s'", tl_repr(unhashable));
    return tlINT(hash);
}

//. size(object): return the size of the object (the count of key/value pairs)
static tlHandle _Object_size(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlINT(tlObjectSize(object));
}

//. keys(object): return a set of its keys
static tlHandle _Object_keys(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlObjectKeys(object);
}

//. values(object): return a list of its values
static tlHandle _Object_values(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlObjectValues(object);
}

//. toMap(object): turn an object into a #Map
static tlHandle _Object_toMap(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");
    return tlMapFromObject(object);
}

// TODO add many keys
//. has(object, key): returns true if an object contains a key
static tlHandle _Object_has(tlTask* task, tlArgs* args) {
    trace("");
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a String");
    tlHandle res = tlObjectGetSym(object, tlSymFromString(key));
    return tlBOOL(res != null);
}

//. get(object, key): returns the value set under the key, undefined if it doesn't have such a mapping
static tlHandle _Object_get(tlTask* task, tlArgs* args) {
    trace("");
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a String");
    tlHandle res = tlObjectGetSym(object, tlSymFromString(key));
    return tlOR_UNDEF(res);
}

// TODO optimize, no need for the intermediate values ...
//. set(object, ((key, value)|object)*): set keys to values, returns a new #Object
//. can also pass in one or more #Objects, they will all be merged into a final #Object
//. > Object.set({x=10,y=10},{x=42},{x=100}) #> {x=100,y=10}
static tlHandle _Object_set(tlTask* task, tlArgs* args) {
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlObjectAs(tlArgsGet(args, 0));

    int size = tlArgsSize(args);
    for (int i = 1; i < size; i++) {
        tlHandle a = tlArgsGet(args, i);
        assert(a);
        tlObject* from = tlObjectCast(a);
        if (from) {
            for (int j = 0; j < 1000; j++) {
                tlHandle key = tlObjectKeyIter(from, j);
                if (!key) break;
                tlHandle val = tlObjectValueIter(from, j);
                assert(val);
                object = tlObjectSet(object, key, val);
            }
            continue;
        }

        tlString* key = tlStringCast(a);
        if (!key) TL_THROW("Expected an Object or key/value pair, not '%s'", tl_repr(a));
        i += 1;
        tlHandle val = tlArgsGet(args, i);
        if (!val) TL_THROW("Expected a value for key '%s'", tl_str(key));
        object = tlObjectToObject_(tlObjectSet(object, tlSymFromString(key), val));
    }
    return object;
}

//. del(*keys): delete all keys passed in and return a new object
static tlHandle _Object_del(tlTask* task, tlArgs* args) {
    if (!tlObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected an Object");
    tlObject* object = tlObjectAs(tlArgsGet(args, 0));

    int size = tlArgsSize(args);
    for (int i = 1; i < size; i++) {
        tlHandle a = tlArgsGet(args, i);
        tlString* key = tlStringCast(a);
        if (!key) {
            if (!tl_bool(a)) continue;
            TL_THROW("Expected a String key or falsy value, not '%s'", tl_repr(a));
        }
        object = tlObjectDel(object, key);
    }
    return object;
}

//. merge(*objects): merge all the objects recursively
//. > Object.merge({a={x=10}},{a={y=20}})
//. >> {a={x=10,y=20}}
static tlHandle _Object_merge(tlTask* task, tlArgs* args) {
    tlObject* object = tlObjectCast(tlArgsGet(args, 0));
    if (!object) TL_THROW("Expected an Object");

    int size = tlArgsSize(args);
    for (int i = 1; i < size; i++) {
        tlHandle a = tlArgsGet(args, i);
        tlObject* other = tlObjectCast(a);
        if (!other) {
            if (!tl_bool(a)) continue;
            TL_THROW("Expected a Object, not '%s'", tl_repr(a));
        }
        object = tlObjectMerge(object, other);
    }
    return object;
}

static size_t objectSize(tlHandle v) {
    return sizeof(tlObject) + sizeof(tlHandle) * tlObjectAs(v)->keys->size;
}
const char* objecttoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Object@%p %d>", v, tlObjectSize(tlObjectAs(v))); return buf;
}

tlHandle objectResolve(tlObject* object, tlSym msg) {
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
    tlSym msg = tlArgsMethod(args);
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

static uint32_t objectHash(tlHandle v, tlHandle* unhashable) {
    tlObject* object = tlObjectAs(v);
    return tlObjectHash(object, unhashable);
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

tlHandle _bless(tlTask* task, tlArgs* args) {
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

tlKind* tlUserClassKind;
tlKind* tlUserObjectKind;
tlKind* tlMutableUserObjectKind;

tlUserClass* tlUserClassFor(tlUserObject* oop) {
    assert(oop);
    return oop->cls;
}

// merge base classes mutable and fields into a single class
tlSet* collectFieldsAndMutable(tlTask* task, tlList* extends, tlSet* fields, bool* mutable) {
    for (int i = 0; i < tlListSize(extends); i++) {
        tlUserClass* cls = tlUserClassCast(tlListGet(extends, i));
        if (!cls) {
            warning("not a class: %s", tl_str(tlListGet(extends, i)));
            continue;
        }

        if (cls->mutable) *mutable = true;
        fields = tlSetUnion(fields, cls->fields);
    }
    return fields;
}

static bool isa(tlUserClass* base, tlUserClass* cls) {
    if (base == cls) return true;
    if (!cls) return false;
    for (int i = 0; i < tlListSize(cls->extends); i++) {
        if (isa(base, tlUserClassCast(tlListGet(cls->extends, i)))) return true;
    }
    return false;
}

// build a class from compiler generated pieces, a constructor, the fields, the methods, the extended objects, etc.
static tlHandle _UserClass_call(tlTask* task, tlArgs* args) {
    tlSym name = tlSymCast_(tlArgsGet(args, 0));
    bool mutable = tl_bool(tlArgsGet(args, 1));
    tlHandle constructor = tlArgsGet(args, 2);
    tlList* extends = tlListCast(tlArgsGet(args, 3));
    tlSet* fields = tlSetFromList(tlListCast(tlArgsGet(args, 4)));
    tlObject* methods = tlObjectCast(tlArgsGet(args, 5));
    tlObject* classfields = tlObjectCast(tlArgsGet(args, 6));
    assert(constructor);
    assert(extends);
    assert(fields);
    assert(methods);
    assert(classfields);

    // ensure the class has all fields and mutability from extended classes
    tlSet* allfields = collectFieldsAndMutable(task, extends, fields, &mutable);
    if (!allfields) return null; // above can throw
    assert(tlSetSize(allfields) >= tlSetSize(fields));
    trace("CLASS: %s fields=%s mutable=%d", tl_str(name), tl_repr(allfields), mutable);

    tlUserClass* cls = tlAlloc(tlUserClassKind, sizeof(tlUserClass));
    cls->name = name;
    cls->mutable = mutable;
    cls->constructor = constructor;
    cls->extends = extends;
    cls->fields = allfields;
    cls->methods = methods;
    cls->classfields = classfields;

    return cls;
}

// create a new class, by invoke the constructor
static tlHandle _userclass_call(tlTask* task, tlArgs* args) {
    TL_TARGET(tlUserClass, cls);
    assert(cls->fields);
    int fields = tlSetSize(cls->fields);
    tlKind* kind = cls->mutable? tlMutableUserObjectKind : tlUserObjectKind;
    tlUserObject* target = tlAlloc(kind, sizeof(tlUserObject) + sizeof(tlHandle) * fields);
    target->cls = cls;

    // constructors are invoke using args(class, ..., this=target)
    return tlInvoke(task, tlArgsFromPrepend1(args, cls->constructor, cls->name, target, cls));
}


// called when a class extends a base class, in the constructor
static tlHandle _UserClass_extend(tlTask* task, tlArgs* args) {
    tlUserClass* cls = tlUserClassCast(tlArgsGet(args, 0));
    tlUserObject* target = tlUserObjectCast(tlArgsGet(args, 1));
    int super = tl_int_or(tlArgsGet(args, 2), -1);
    assert(cls);
    assert(tlListIs(cls->extends));
    assert(target);
    assert(super >= 0);
    tlUserClass* extend = tlUserClassAs(tlListGet(cls->extends, super));
    assert(extend);

    // constructors are invoke using args(class, ..., this=target), notice this.cls != class, but this.extends(class)
    tlList* names = tlArgsNames(args);
    tlArgs* args2 = tlArgsNewNames(tlArgsRawSize(args) - 2, !!names, true);
    tlArgsSetFn_(args2, extend->constructor);
    tlArgsSetMethod_(args2, extend->name);
    tlArgsSetTarget_(args2, target);
    if (names) tlArgsSetNames_(args2, names, tlArgsNamedSize(args));
    tlArgsSet_(args2, 0, extend);
    for (int i = 3; i < tlArgsRawSize(args); i++) {
        tlArgsSet_(args2, i - 2, tlArgsGetRaw(args, i));
    }
    return tlInvoke(task, args2);
}

// called when a constructor assigns a field
static tlHandle _UserClass_setField(tlTask* task, tlArgs* args) {
    tlUserObject* oop = tlUserObjectCast(tlArgsGet(args, 0));
    assert(oop);
    assert(oop->cls);
    assert(tlSetIs(oop->cls->fields));
    tlSym name = tlSymAs_(tlArgsGet(args, 1));
    tlHandle v = tlOR_NULL(tlArgsGet(args, 2));

    tlUserClass* cls = oop->cls;
    trace("setField: %s.%s = %s", tl_str(oop), tl_str(name), tl_str(v));
    int field = tlSetIndexof(cls->fields, name);
    if (field < 0) TL_THROW("not a field: '%s'", tl_str(name));
    assert(field < tlSetSize(cls->fields));
    oop->fields[field] = v;
    return v;
}

static tlHandle tlUserClassMethodResolve(tlUserClass* cls, tlSym name);

static tlHandle tlUserClassListMethodResolve(tlList* extends, tlSym name) {
    int size = tlListSize(extends);
    for (int i = 0; i < size; i++) {
        tlUserClass* cls = tlUserClassCast(tlListGet(extends, i));
        if (!cls) continue;
        tlHandle method = tlUserClassMethodResolve(cls, name);
        if (method) return method;
    }
    return null;
}

static tlHandle tlUserClassMethodResolve(tlUserClass* cls, tlSym name) {
    tlHandle method = tlObjectGet(cls->methods, name);
    if (method) return method;
    return tlUserClassListMethodResolve(cls->extends, name);
}

static tlHandle _UserClass_super(tlTask* task, tlArgs* args) {
    tlUserObject* oop = tlUserObjectCast(tlArgsGet(args, 0));
    if (!oop) TL_THROW("not an class based object");
    tlSym name = tlSymAs_(tlArgsGet(args, 1));
    if (!name) TL_THROW("require a name");

    tlList* extends = oop->cls->extends;
    tlHandle method = tlUserClassListMethodResolve(extends, name);
    if (method) return method;

    // every class commutes its extends, so we only have to check the first level
    int size = tlListSize(extends);
    for (int i = 0; i < size; i++) {
        tlUserClass* cls = tlUserClassCast(tlListGet(extends, i));
        if (!cls) continue;

        if (tlSetIndexof(cls->fields, name) >= 0) {
            int field = tlSetIndexof(oop->cls->fields, name);
            assert(field >= 0);
            return tlBLazyDataNew(tlOR_NULL(oop->fields[field]));
        }
    }
    TL_THROW("'%s' is not a property of super('%s')", tl_str(name), tl_str(oop));
}

static tlHandle _UserClass_isa(tlTask* task, tlArgs* args) {
    tlUserClass* cls = tlUserClassCast(tlArgsGet(args, 0));
    if (!cls) TL_THROW("isa expects a class as first argument");
    tlUserObject* oop = tlUserObjectCast(tlArgsGet(args, 1));
    return tlBOOL(oop && isa(cls, oop->cls));
}

static tlHandle _isUserClass(tlTask* task, tlArgs* args) {
    return tlBOOL(tlUserClassIs(tlArgsGet(args, 0)));
}

static tlHandle g_userobject_set;
static tlHandle _userobject__set(tlTask* task, tlArgs* args) {
    TL_TARGET(tlUserObject, oop);
    tlUserClass* cls = oop->cls;
    if (!cls->mutable) TL_THROW("not a mutable object: '%s'", tl_str(oop));
    tlSym name = tlSymCast(tlArgsGet(args, 0));
    tlHandle v = tlOR_NULL(tlArgsGet(args, 1));

    int field = tlSetIndexof(cls->fields, name);
    if (field < 0) TL_THROW("not a field: '%s'", tl_str(name));
    assert(field < tlSetSize(cls->fields));
    oop->fields[field] = v;
    return v;
}

static tlHandle _userclass_name(tlTask* task, tlArgs* args) {
    TL_TARGET(tlUserClass, cls);
    return cls->name;
}

uint32_t tlUserObjectHash(tlUserObject* object, tlHandle* unhashable) {
    // if (object->hash) return object->hash;
    uint32_t hash = 4280703812; // 12.hash + 1
    uint32_t size = tlSetSize(object->cls->fields);
    for (uint32_t i = 0; i < size; i++) {
        uint32_t k = tlHandleHash(tlSetGet(object->cls->fields, i), unhashable);
        uint32_t v = tlHandleHash(object->fields[i], unhashable);
        if (!k || !v) return 0;
        // rotate shift the hash then mix in the key, rotate, then value
        hash = hash << 1 | hash >> 31;
        hash ^= k;
        hash = hash << 1 | hash >> 31;
        hash ^= v;
    }
    // and mix in the list size after a murmur
    hash ^= murmurhash2a(&size, sizeof(size));
    return hash | !hash;
}

tlHandle userclassResolveStatic(tlUserClass* cls, tlSym name) {
    if (name == s_class) return _tl_class;
    return tlObjectGet(cls->classfields, name);
}

// resolve a name to a method, walking up the super hierarchy as needed; methods before fields
tlHandle userobjectResolve(tlUserObject* oop, tlSym name) {
    tlHandle method = tlUserClassMethodResolve(oop->cls, name);
    if (method) return method;

    int field = tlSetIndexof(oop->cls->fields, name);
    if (field < 0) {
        if (name != s__set) return null;
        return g_userobject_set;
    }
    assert(field >= 0 && field < tlSetSize(oop->cls->fields));
    return tlOR_NULL(oop->fields[field]);
}

const char* userclasstoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Class@%p %s>", v, tl_str(tlUserClassAs(v)->name)); return buf;
}

const char* userobjecttoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<%s@%p>", tl_str(tlUserObjectAs(v)->cls->name), v); return buf;
}

static uint32_t userobjectHash(tlHandle v, tlHandle* unhashable) {
    tlUserObject* object = tlUserObjectAs(v);
    return tlUserObjectHash(object, unhashable);
}
static bool userobjectEquals(tlHandle _left, tlHandle _right) {
    if (_left == _right) return true;

    tlUserObject* left = tlUserObjectAs(_left);
    tlUserObject* right = tlUserObjectAs(_right);
    if (left->cls != right->cls) return false;

    int size = tlSetSize(left->cls->fields);
    for (int i = 0; i < size; i++) {
        if (!tlHandleEquals(left->fields[i], right->fields[i])) return false;
    }
    return true;
}
static tlHandle userobjectCmp(tlHandle _left, tlHandle _right) {
    if (_left == _right) return tlEqual;

    tlUserObject* left = tlUserObjectAs(_left);
    tlUserObject* right = tlUserObjectAs(_right);

    // TODO this is runtime dependent, doesn't have to be
    if (left->cls < right->cls) return tlSmaller;
    if (left->cls > right->cls) return tlLarger;

    assert(left->cls == right->cls);

    int size = tlSetSize(left->cls->fields);
    for (int i = 0; i < size; i++) {
        tlHandle cmp = tlHandleCompare(left->fields[i], right->fields[i]);
        if (cmp != tlEqual) return cmp;
    }
    assert(userobjectEquals(left, right));
    return tlEqual;
}

void class_init_first() {
    INIT_KIND(tlClassKind);
    INIT_KIND(tlObjectKind);
}

void object_init() {
    s_methods = tlSYM("methods");
    tlObject* constructor = tlClassObjectFrom(
        "call", _Object_from,
        "hash", _Object_hash,
        "size", _Object_size,
        "has", _Object_has,
        "get", _Object_get,
        "set", _Object_set,
        "del", _Object_del,
        "keys", _Object_keys,
        "values", _Object_values,
        "toMap", _Object_toMap,
        "merge", _Object_merge,
        "inherit", _Object_inherit,
        "each", null,
        "map", null,
        "filter", null,
        null
    );
    tl_register_global("Object", constructor);
    _tl_emptyObject = tlObjectNew(tlSetEmpty());
    assert(tlSetIs(tlSetEmpty()));
    assert(tlObjectIs(tlObjectEmpty()));


    tlClass* cls = tlCLASS("UserClass", tlNATIVE(_UserClass_call, "UserClass"),
    tlMETHODS(
        "name", _userclass_name,
        "call", _userclass_call,
        null
    ), null);
    tlKind _tlUserClassKind = {
        .name = "UserClass",
        .cls = cls,
        .toString = userclasstoString,
    };
    INIT_KIND(tlUserClassKind);
    tl_register_global("UserClass", cls);

    tlKind _tlUserObjectKind = {
        .name = "UserObject",
        .toString = userobjecttoString,
        .hash = userobjectHash,
        .equals = userobjectEquals,
        .cmp = userobjectCmp,
    };
    INIT_KIND(tlUserObjectKind);
    tlKind _tlMutableUserObjectKind = {
        .locked = true,
        .name = "MutableUserObject",
        .toString = userobjecttoString,
    };
    INIT_KIND(tlMutableUserObjectKind);

    assert(s__set);
    g_userobject_set = tlNativeNew(_userobject__set, s__set);
    tl_register_global("_class", tlNativeNew(_UserClass_call, tlSYM("_class")));
    tl_register_global("_setfield", tlNativeNew(_UserClass_setField, tlSYM("_setfield")));
    tl_register_global("_extend", tlNativeNew(_UserClass_extend, tlSYM("_extend")));
    tl_register_global("_super", tlNativeNew(_UserClass_super, tlSYM("_super")));
    tl_register_global("_isa", tlNativeNew(_UserClass_isa, tlSYM("_isa")));
    tl_register_global("isUserClass", tlNativeNew(_isUserClass, tlSYM("isUserClass")));
}

