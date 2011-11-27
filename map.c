// a map implementation

#include "trace-on.h"

static tlClass _tlMapClass;
static tlClass _tlValueObjectClass;
tlClass* tlMapClass = &_tlMapClass;
tlClass* tlValueObjectClass = &_tlValueObjectClass;

tlMap* tlMapFromObjectAs(tlValue v) { assert(tlValueObjectIs(v)); return (tlMap*)v; }
tlMap* tlMapFromObjectCast(tlValue v) { return tlValueObjectIs(v)?(tlMap*)v:null; }
bool tlMapOrObjectIs(tlValue v) { return tlMapIs(v) || tlValueObjectIs(v); }

static tlMap* _tl_emptyMap;

struct tlMap {
    tlHead head;
    tlSet* keys;
    tlValue data[];
};

tlMap* tlMapEmpty() { return _tl_emptyMap; }

tlMap* tlMapNew(tlTask* task, tlSet* keys) {
    if (!keys) keys = _tl_set_empty;
    tlMap* map = tlAllocWithFields(task, tlMapClass, sizeof(tlMap), tlSetSize(keys));
    map->keys = keys;
    assert(tlMapSize(map) == tlSetSize(keys));
    return map;
}
tlMap* tlMapToObject_(tlMap* map) {
    map->head.klass = tlValueObjectClass; return map;
}
int tlMapSize(tlMap* map) {
    assert(tlMapOrObjectIs(map));
    return map->head.size;
}
tlSet* tlMapKeySet(tlMap* map) {
    return map->keys;
}
void tlMapDump(tlMap* map) {
    print("---- MAP DUMP @ %p ----", map);
    for (int i = 0; i < tlMapSize(map); i++) {
        print("%d %s: %s", i, tl_str(map->keys->data[i]), tl_str(map->data[i]));
    }
    print("----");
}

tlValue tlMapGet(tlTask* task, tlMap* map, tlValue key) {
    assert(tlMapOrObjectIs(map));
    int at = tlSetIndexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlMapSize(map));
    return map->data[at];
}
tlMap* tlMapSet(tlTask* task, tlMap* map, tlValue key, tlValue v) {
    trace("set map: %s = %s", tl_str(key), tl_str(v));

    int at = 0;
    at = tlSetIndexof(map->keys, key);
    if (at >= 0) {
        tlMap* nmap = tlAllocClone(task, map, sizeof(tlMap));
        nmap->data[at] = v;
        return nmap;
    }

    int size = tlSetSize(map->keys) + 1;
    tlSet* keys = tlSetCopy(task, map->keys, size);
    at = tlSetAdd_(keys, key);

    tlMap* nmap = tlMapNew(task, keys);
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

tlValue tlMapGetSym(tlMap* map, tlSym key) {
    assert(tlMapOrObjectIs(map));
    int at = tlSetIndexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlMapSize(map));
    trace("keys get: %s = %s", tl_str(key), tl_str(map->data[at]));
    return map->data[at];
}
void tlMapSetSym_(tlMap* map, tlSym key, tlValue v) {
    assert(tlMapOrObjectIs(map));
    int at = tlSetIndexof(map->keys, key);
    assert(at >= 0 && at < tlMapSize(map));
    trace("keys set_: %s = %s", tl_str(key), tl_str(map->data[at]));
    map->data[at] = v;
}

tlMap* tlMapFromPairs(tlTask* task, tlList* pairs) {
    int size = tlListSize(pairs);
    trace("%d", size);

    tlSet* keys = tlSetNew(task, size);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlSymAs(tlListGet(pair, 0));
        tlSetAdd_(keys, name);
    }

    tlMap* map = tlMapNew(task, keys);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlSymAs(tlListGet(pair, 0));
        tlValue v = tlListGet(pair, 1);
        tlMapSetSym_(map, name, v);
    }
    return map;
}

tlValue tlMapValueIter(tlMap* map, int i) {
    assert(i >= 0);
    if (i >= tlMapSize(map)) return null;
    return map->data[i];
}

void tlMapValueIterSet_(tlMap* map, int i, tlValue v) {
    assert(i >= 0 && i < tlMapSize(map));
    map->data[i] = v;
}

tlMap* tlObjectFrom(tlTask* task, ...) {
    va_list ap;
    int size = 1;

    va_start(ap, task);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlValue v = va_arg(ap, tlValue); assert(v);
        size++;
    }
    va_end(ap);

    tlSet* keys = tlSetNew(task, size);
    va_start(ap, task);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlValue);
        tlSetAdd_(keys, tlSYM(n));
    }
    va_end(ap);

    tlMap* map = tlMapNew(task, keys);
    va_start(ap, task);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlValue v = va_arg(ap, tlValue); assert(v);
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
        tlNativeCb fn = va_arg(ap, tlNativeCb); assert(fn);
        size++;
    }
    va_end(ap);

    tlSet* keys = tlSetNew(null, size);
    tlSetAdd_(keys, tlSYM(n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlNativeCb);
        tlSetAdd_(keys, tlSYM(n));
    }
    va_end(ap);

    tlMap* map = tlMapNew(null, keys);
    tlMapSetSym_(map, tlSYM(n1), tlNATIVE(fn1, n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlNativeCb fn = va_arg(ap, tlNativeCb);
        tlMapSetSym_(map, tlSYM(n), tlNATIVE(fn, n));
    }
    va_end(ap);

    return tlMapToObject_(map);
}

// called when map literals contain lookups or expressions to evaluate
static tlValue _Map_clone(tlTask* task, tlArgs* args) {
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    int size = tlMapSize(map);
    map = tlAllocClone(task, map, sizeof(tlMap));
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!map->data[i] || map->data[i] == tlUndefined) {
            map->data[i] = tlArgsGet(args, argc++);
        }
    }
    assert(argc == tlArgsSize(args));
    return map;
}
// TODO these functions create way to many intermediates ... add more code :(
static tlValue _Map_update(tlTask* task, tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    if (!tlMapOrObjectIs(tlArgsGet(args, 1))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    tlClass* klass = map->head.klass;
    tlMap* add = tlArgsGet(args, 1);
    for (int i = 0; i < add->head.size; i++) {
        map = tlMapSet(task, map, add->keys->data[i], add->data[i]);
    }
    map->head.klass = klass;
    return map;
}
static tlValue _Map_inherit(tlTask* task, tlArgs* args) {
    trace("");
    if (!tlMapOrObjectIs(tlArgsGet(args, 0))) TL_THROW("Expected a Map");
    tlMap* map = tlArgsGet(args, 0);
    tlValue oclass = tlMapGetSym(map, s_class);
    tlClass* klass = map->head.klass;
    for (int i = 1; i < tlArgsSize(args); i++) {
        if (!tlMapOrObjectIs(tlArgsGet(args, i))) TL_THROW("Expected a Map");
        tlMap* add = tlArgsGet(args, 1);
        for (int i = 0; i < add->head.size; i++) {
            map = tlMapSet(task, map, add->keys->data[i], add->data[i]);
        }
    }
    if (oclass) map = tlMapSet(task, map, s_class, oclass);
    map->head.klass = klass;
    return map;
}
static tlValue _map_size(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    return tlINT(tlMapSize(map));
}
static tlValue _map_get(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlValue key = tlArgsGet(args, 0);
    if (!key) TL_THROW("Excpected a key");
    tlValue res = tlMapGet(task, map, key);
    if (!res) return tlUndefined;
    return res;
}
static tlValue _map_set(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlValue key = tlArgsGet(args, 0);
    if (!key) TL_THROW("Expected a key");
    tlValue val = tlArgsGet(args, 1);
    if (!val || val == tlUndefined) val = tlNull;
    tlMap* nmap = tlMapSet(task, map, key, val);
    return nmap;
}
static tlValue _map_toObject(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlMap* nmap = tlMapNew(task, map->keys);
    for (int i = 0; i < tlMapSize(map); i++) nmap->data[i] = map->data[i];
    return tlMapToObject_(nmap);
}

const char* mapToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Map@%p %d>", v, tlMapSize(tlMapAs(v))); return buf;
}
static tlClass _tlMapClass = {
    .name = "text",
    .toText = mapToText,
};

const char* valueObjectToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<ValueObject@%p %d>", v, tlMapSize(tlMapFromObjectAs(v))); return buf;
}
static tlValue valueObjectSend(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapFromObjectCast(tlArgsTarget(args));
    tlSym msg = tlArgsMsg(args);

    tlValue field = tlMapGet(task, map, msg);
    trace("VALUE SEND %p: %s -> %s", map, tl_str(msg), tl_str(field));
    if (!field) {
        do {
            tlMap* klass = tlMapGet(task, map, s_class);
            trace("CLASS: %s", tl_str(klass));
            if (!klass) TL_THROW("'%s' is undefined", tl_str(msg));
            field = tlMapGet(task, klass, msg);
            if (field) {
                // TODO maybe just try, and catch not callable exceptions?
                if (!tlCallableIs(field)) return field;
                return tlEvalArgsFn(task, args, field);
            }
            map = klass;
        } while (true);
        return tlUndefined;
    }
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(task, args, field);
}
static tlValue valueObjectRun(tlTask* task, tlValue fn, tlArgs* args) {
    tlMap* map = tlMapFromObjectAs(fn);
    tlValue field = tlMapGetSym(map, s_call);
    if (field) {
        trace("%s", tl_str(field));
        // TODO this is awkward, tlCallableIs does not know about complex user objects
        if (!tlCallableIs(field)) return field;
        return tlEvalArgsTarget(task, args, fn, field);
    }
    TL_THROW("'%s' not callable", tl_str(fn));
}
static tlClass _tlValueObjectClass = {
    .name = "ValueObject",
    .toText = valueObjectToText,
    .send = valueObjectSend,
    .run = valueObjectRun,
};

static void map_init() {
    _tlMapClass.map = tlClassMapFrom(
            "size", _map_size,
            "get", _map_get,
            "set", _map_set,
            "toObject", _map_toObject,
            null
    );
    _tl_emptyMap = tlMapNew(null, null);
}

