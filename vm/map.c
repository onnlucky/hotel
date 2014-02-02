// a map implementation

#include "trace-off.h"

static tlKind _tlMapKind;
static tlKind _tlHandleObjectKind;
tlKind* tlMapKind = &_tlMapKind;
tlKind* tlHandleObjectKind = &_tlHandleObjectKind;

tlMap* tlMapOrObjectAs(tlHandle v) { assert(tlMapIs(v) || tlHandleObjectIs(v)); return (tlMap*)v; }
tlMap* tlMapOrObjectCast(tlHandle v) { if (tlMapIs(v) || tlHandleObjectIs(v)) return (tlMap*)v; return null; }
tlMap* tlMapFromObjectAs(tlHandle v) { assert(tlHandleObjectIs(v)); return (tlMap*)v; }
tlMap* tlMapFromObjectCast(tlHandle v) { return tlHandleObjectIs(v)?(tlMap*)v:null; }
bool tlMapOrObjectIs(tlHandle v) { return tlMapIs(v) || tlHandleObjectIs(v); }

struct tlMap {
    tlHead head;
    tlSet* keys;
    tlHandle data[];
};

static tlMap* _tl_emptyMap;

tlMap* tlMapEmpty() { return tlMapNew(null); }

tlMap* tlMapNew(tlSet* keys) {
    if (!keys) keys = _tl_set_empty;
    tlMap* map = tlAlloc(tlMapKind, sizeof(tlMap) + sizeof(tlHandle) * keys->size);
    map->keys = keys;
    assert(tlMapSize(map) == tlSetSize(keys));
    return map;
}
tlMap* tlMapToObject_(tlMap* map) {
    assert((map->head.kind & 0x7) == 0);
    map->head.kind = (intptr_t)tlHandleObjectKind; return map;
}
int tlMapHash(tlMap* map) {
    // if (map->hash) return map->hash;
    unsigned int hash = 0x1;
    for (int i = 0; i < map->keys->size; i++) {
        hash ^= tlHandleHash(tlSetGet(map->keys, i));
        hash ^= tlHandleHash(map->data[i]);
    }
    if (!hash) hash = 1;
    return hash;
}
int tlMapSize(const tlMap* map) {
    return map->keys->size;
}
tlSet* tlMapKeys(const tlMap* map) {
    return map->keys;
}
tlList* tlMapValues(const tlMap* map) {
    tlList* list = tlListNew(tlMapSize(map));
    for (int i = 0; i < map->keys->size; i++) {
        assert(map->data[i]);
        tlListSet_(list, i, map->data[i]);
    }
    return list;
}
void tlMapDump(tlMap* map) {
    print("---- MAP DUMP @ %p ----", map);
    for (int i = 0; i < tlMapSize(map); i++) {
        print("%d %s: %s", i, tl_str(map->keys->data[i]), tl_str(map->data[i]));
    }
    print("----");
}

tlMap* tlMapToObject(tlMap* map) {
    tlMap* nmap = tlMapNew(map->keys);
    for (int i = 0; i < tlMapSize(map); i++) nmap->data[i] = map->data[i];
    return tlMapToObject_(nmap);
}
tlMap* tlObjectToMap(tlMap* map) {
    tlMap* nmap = tlMapNew(map->keys);
    for (int i = 0; i < tlMapSize(map); i++) nmap->data[i] = map->data[i];
    return nmap;
}

tlHandle tlMapGet(tlMap* map, tlHandle key) {
    assert(tlMapOrObjectIs(map));
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlMapSize(map));
    return map->data[at];
}
tlMap* tlMapSet(tlMap* map, tlHandle key, tlHandle v) {
    if (tlStringIs(key)) key = tlSymFromString(key);
    trace("set map: %s = %s", tl_str(key), tl_str(v));

    int at = 0;
    at = tlSetIndexof(map->keys, key);
    if (at >= 0) {
        tlMap* nmap = tlClone(map);
        nmap->data[at] = v;
        return nmap;
    }

    int size = tlSetSize(map->keys) + 1;
    tlSet* keys = tlSetCopy(map->keys, size);
    at = tlSetAdd_(keys, key);

    tlMap* nmap = tlMapNew(keys);
    int i;
    for (i = 0; i < at; i++) {
        nmap->data[i] = map->data[i];
    }
    nmap->data[at] = v; i++;
    for (; i < size; i++) {
        nmap->data[i] = map->data[i - 1];
    }
    return nmap;
}

tlHandle tlMapGetSym(const tlMap* map, tlHandle key) {
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlMapSize(map));
    trace("keys get: %s = %s", tl_str(key), tl_str(map->data[at]));
    return map->data[at];
}
void tlMapSetSym_(tlMap* map, tlHandle key, tlHandle v) {
    assert(tlMapOrObjectIs(map));
    if (tlStringIs(key)) key = tlSymFromString(key);
    int at = tlSetIndexof(map->keys, key);
    assert(at >= 0 && at < tlMapSize(map));
    trace("keys set_: %s = %s", tl_str(key), tl_str(map->data[at]));
    map->data[at] = v;
}

tlMap* tlMapFromPairs(tlList* pairs) {
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

    tlMap* map = tlMapNew(keys);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlSymAs(tlListGet(pair, 0));
        tlHandle v = tlListGet(pair, 1);
        tlMapSetSym_(map, name, v);
    }
    return map;
}

tlHandle tlMapValueIter(const tlMap* map, int i) {
    assert(i >= 0);
    if (i >= tlMapSize(map)) return null;
    return map->data[i];
}
tlHandle tlMapKeyIter(const tlMap* map, int i) {
    assert(i >= 0);
    if (i >= tlMapSize(map)) return null;
    return map->keys->data[i];
}

void tlMapValueIterSet_(tlMap* map, int i, tlHandle v) {
    assert(i >= 0 && i < tlMapSize(map));
    map->data[i] = v;
}

tlMap* tlObjectFrom(const char* n1, tlHandle v1, ...) {
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

    tlMap* map = tlMapNew(keys);
    tlMapSetSym_(map, tlSYM(n1), v1);
    va_start(ap, v1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlHandle v = va_arg(ap, tlHandle); assert(v);
        tlMapSetSym_(map, tlSYM(n), v);
    }
    va_end(ap);

    return tlMapToObject_(map);
}

tlMap* tlClassMapFrom(const char* n1, tlNativeCb fn1, ...) {
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

    tlMap* map = tlMapNew(keys);
    tlMapSetSym_(map, tlSYM(n1), tlNATIVE(fn1, n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlNativeCb fn = va_arg(ap, tlNativeCb);
        if (fn) tlMapSetSym_(map, tlSYM(n), tlNATIVE(fn, n));
    }
    va_end(ap);

    return tlMapToObject_(map);
}

// called when map literals contain lookups or expressions to evaluate
static tlHandle _Map_clone(tlArgs* args) {
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    int size = tlMapSize(map);
    map = tlClone(map);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!map->data[i] || tlUndefinedIs(map->data[i])) {
            map->data[i] = tlArgsGet(args, argc++);
        }
    }
    assert(argc == tlArgsSize(args));
    return map;
}

tlMap* tlHashMapToMap(tlHashMap*);
tlMap* tlHashMapToObject(tlHashMap*);
static tlHandle _Map_from(tlArgs* args) {
    tlHandle a1 = tlArgsGet(args, 0);
    if (tlHashMapIs(a1)) return tlHashMapToMap(a1);
    if (tlMapOrObjectIs(a1)) return tlObjectToMap(a1);
    TL_THROW("expect a object, map, or HashMap");
}
static tlHandle _Object_from(tlArgs* args) {
    tlHandle a1 = tlArgsGet(args, 0);
    if (tlHashMapIs(a1)) return tlHashMapToObject(a1);
    if (tlMapOrObjectIs(a1)) return tlMapToObject(a1);
    TL_THROW("expect a object, map, or HashMap");
}

// TODO these functions create way to many intermediates ... add more code :(
static tlHandle _Map_update(tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    if (!tlMapOrObjectIs(tlArgsGet(args, 1))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    intptr_t kind = map->head.kind;
    tlMap* add = tlArgsGet(args, 1);
    for (int i = 0; i < add->keys->size; i++) {
        map = tlMapSet(map, add->keys->data[i], add->data[i]);
    }
    map->head.kind = kind;
    return map;
}
static tlHandle _Map_inherit(tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    tlHandle oclass = tlMapGetSym(map, s_class);
    intptr_t kind = map->head.kind;
    for (int i = 1; i < tlArgsSize(args); i++) {
        if (!tlMapOrObjectIs(tlArgsGet(args, i))) TL_THROW("Expected a Map");
        tlMap* add = tlArgsGet(args, 1);
        for (int i = 0; i < add->keys->size; i++) {
            map = tlMapSet(map, add->keys->data[i], add->data[i]);
        }
    }
    if (oclass) map = tlMapSet(map, s_class, oclass);
    map->head.kind = kind;
    return map;
}
static tlHandle _Map_hash(tlArgs* args) {
    tlMap* map = tlMapOrObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("Expected a Map");
    return tlINT(tlMapHash(map));
}
static tlHandle _Map_size(tlArgs* args) {
    tlMap* map = tlMapOrObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("Expected a Map");
    return tlINT(tlMapSize(map));
}
static tlHandle _Map_keys(tlArgs* args) {
    tlMap* map = tlMapOrObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("Expected a Map");
    return tlMapKeys(map);
}
static tlHandle _Map_values(tlArgs* args) {
    tlMap* map = tlMapOrObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("Expected a Map");
    return tlMapValues(map);
}
static tlHandle _Map_get(tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a symbol");
    tlHandle res = tlMapGetSym(map, tlSymFromString(key));
    return tlMAYBE(res);
}
static tlHandle _Object_set(tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a symbol");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("Expected a value");
    return tlMapToObject_(tlMapSet(map, tlSymFromString(key), val));
}
static tlHandle _Map_set(tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    tlString* key = tlStringCast(tlArgsGet(args, 1));
    if (!key) TL_THROW("Expected a symbol");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) TL_THROW("Expected a value");
    return tlMapSet(map, tlSymFromString(key), val);
}
static tlHandle _map_hash(tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlINT(tlMapHash(map));
}
static tlHandle _map_size(tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    return tlINT(tlMapSize(map));
}
static tlHandle _map_keys(tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlMapKeys(map);
}
static tlHandle _map_values(tlArgs* args) {
    tlMap* map = tlMapAs(tlArgsTarget(args));
    return tlMapValues(map);
}
static tlHandle _map_get(tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("Excpected a key");
    tlHandle res = tlMapGet(map, key);
    return tlMAYBE(res);
}
static tlHandle _map_set(tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlHandle key = tlArgsGet(args, 0);
    if (!key) TL_THROW("Expected a key");
    tlHandle val = tlArgsGet(args, 1);
    if (!val || tlUndefinedIs(val)) val = tlNull;
    tlMap* nmap = tlMapSet(map, key, val);
    return nmap;
}
static tlHandle _map_toObject(tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    return tlMapToObject(map);
}
const char* maptoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<Map@%p %d>", v, tlMapSize(tlMapAs(v))); return buf;
}
static size_t mapSize(tlHandle v) {
    return sizeof(tlMap) + sizeof(tlHandle) * tlMapAs(v)->keys->size;
}
static tlKind _tlMapKind = {
    .name = "map",
    .size = mapSize,
    .toString = maptoString,
};

static size_t valueObjectSize(tlHandle v) {
    return sizeof(tlMap) + sizeof(tlHandle) * tlMapFromObjectAs(v)->keys->size;
}
const char* valueObjecttoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "<ValueObject@%p %d>", v, tlMapSize(tlMapFromObjectAs(v))); return buf;
}
static tlHandle valueObjectSend(tlArgs* args) {
    tlMap* map = tlMapFromObjectCast(tlArgsTarget(args));
    tlSym msg = tlArgsMsg(args);
    tlHandle field = null;
    tlMap* cls;

    // search for field
    cls = map;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(msg));
        field = tlMapGet(cls, msg);
        if (field) goto send;
        cls = tlMapGet(cls, s_class);
    }

    // search for getter
    cls = map;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(s__get));
        field = tlMapGet(cls, s__get);
        if (field) goto send;
        cls = tlMapGet(cls, s_class);
    }

send:;
    trace("VALUE SEND %p: %s -> %s", map, tl_str(msg), tl_str(field));
    if (!field) TL_THROW("%s.%s is undefined", tl_str(map), tl_str(msg));
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(args, field);
}
static tlHandle valueObjectRun(tlHandle fn, tlArgs* args) {
    tlMap* map = tlMapFromObjectAs(fn);
    tlHandle field = tlMapGetSym(map, s_call);
    if (field) {
        trace("%s", tl_str(field));
        // TODO this is awkward, tlCallableIs does not know about complex user objects
        if (!tlCallableIs(field)) return field;
        return tlEvalArgsTarget(args, fn, field);
    }
    TL_THROW("'%s' not callable", tl_str(fn));
}

static unsigned int mapHash(tlHandle v) {
    tlMap* map = tlMapOrObjectAs(v);
    return tlMapHash(map);
}
static bool mapEquals(tlHandle _left, tlHandle _right) {
    if (_left == _right) return true;

    tlMap* left = tlMapOrObjectAs(_left);
    tlMap* right = tlMapOrObjectAs(_right);
    if (left->keys->size != right->keys->size) return false;

    for (int i = 0; i < left->keys->size; i++) {
        if (!tlHandleEquals(tlSetGet(left->keys, i), tlSetGet(right->keys, i))) return false;
        if (!tlHandleEquals(left->data[i], right->data[i])) return false;
    }
    return true;
}
static tlHandle mapCmp(tlHandle _left, tlHandle _right) {
    if (_left == _right) return 0;

    tlMap* left = tlMapOrObjectAs(_left);
    tlMap* right = tlMapOrObjectAs(_right);
    int size = MIN(left->keys->size, right->keys->size);
    for (int i = 0; i < size; i++) {
        tlHandle cmp = tlHandleCompare(tlSetGet(left->keys, i), tlSetGet(right->keys, i));
        if (cmp != tlEqual) return cmp;
        cmp = tlHandleCompare(left->data[i], right->data[i]);
        if (cmp != tlEqual) return cmp;
    }
    return tlCOMPARE(left->keys->size - right->keys->size);
}



static tlKind _tlHandleObjectKind = {
    .name = "ValueObject",
    .size = valueObjectSize,
    .hash = mapHash,
    .equals = mapEquals,
    .cmp = mapCmp,
    .toString = valueObjecttoString,
    .send = valueObjectSend,
    .run = valueObjectRun,
};

static void map_init() {
    _tl_emptyMap = tlMapNew(null);
    _tlMapKind.klass = tlClassMapFrom(
        "hash", _map_hash,
        "size", _map_size,
        "get", _map_get,
        "set", _map_set,
        "keys", _map_keys,
        "vales", _map_values,
        "toObject", _map_toObject,
        "each", null,
        null
    );
    tlMap* constructor = tlClassMapFrom(
        "call", _Map_from,
        "hash", _Map_hash,
        "size", _Map_size,
        "get", _Map_get,
        "set", _Map_set,
        "keys", _Map_keys,
        "values", _Map_values,
        "class", null,
        "each", null,
        null
    );
    tlMap* oconstructor = tlClassMapFrom(
        "call", _Object_from,
        "hash", _Map_hash,
        "get", _Map_get,
        "set", _Object_set,
        "keys", _Map_keys,
        "values", _Map_values,
        "each", null,
        null
    );
    tlMapSetSym_(constructor, s_class, _tlMapKind.klass);
    tl_register_global("Map", constructor);
    tl_register_global("Object", oconstructor);
}

