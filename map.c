// a map implementation

#include "trace-off.h"

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

tlMap* tlmap_empty() { return _tl_emptyMap; }

tlMap* tlmap_new(tlTask* task, tlSet* keys) {
    if (!keys) keys = v_set_empty;
    tlMap* map = tlAllocWithFields(task, tlMapClass, sizeof(tlMap), tlset_size(keys));
    map->keys = keys;
    assert(tlmap_size(map) == tlset_size(keys));
    return map;
}
int tlmap_size(tlMap* map) {
    assert(tlMapOrObjectIs(map));
    return map->head.size;
}
tlSet* tlmap_keyset(tlMap* map) {
    return map->keys;
}
void tlmap_dump(tlMap* map) {
    print("---- MAP DUMP @ %p ----", map);
    for (int i = 0; i < tlmap_size(map); i++) {
        print("%d %p: %p", i, map->data[i], map->keys->data[i]);
        print("%d %s: %s", i, tl_str(map->data[i]), tl_str(map->keys->data[i]));
    }
    print("----");
}

tlValue tlmap_get(tlTask* task, tlMap* map, tlValue key) {
    assert(tlMapOrObjectIs(map));
    int at = tlset_indexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlmap_size(map));
    return map->data[at];
}
tlMap* tlmap_set(tlTask* task, tlMap* map, tlValue key, tlValue v) {
    trace("set map: %s = %s", tl_str(key), tl_str(v));
    //if (tlint_is(key)) return tlmap_set_int(task, map, tlint_as(key), v);

    int at = 0;
    at = tlset_indexof(map->keys, key);
    if (at >= 0) {
        tlMap* nmap = TL_CLONE(map);
        nmap->data[at] = v;
        return nmap;
    }

    int size = tlset_size(map->keys) + 1;
    tlSet* keys = tlset_copy(task, map->keys, size);
    at = tlset_add_(keys, key);

    tlMap* nmap = tlmap_new(task, keys);
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

tlValue tlmap_get_sym(tlMap* map, tlSym key) {
    assert(tlMapOrObjectIs(map));
    int at = tlset_indexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlmap_size(map));
    trace("keys get: %s = %s", tl_str(key), tl_str(map->data[at]));
    return map->data[at];
}
void tlmap_set_sym_(tlMap* map, tlSym key, tlValue v) {
    assert(tlMapOrObjectIs(map));
    int at = tlset_indexof(map->keys, key);
    assert(at >= 0 && at < tlmap_size(map));
    trace("keys set_: %s = %s", tl_str(key), tl_str(map->data[at]));
    map->data[at] = v;
}

tlMap* tlmap_from_pairs(tlTask* task, tlList* pairs) {
    int size = tlListSize(pairs);
    trace("%d", size);

    tlSet* keys = tlset_new(task, size);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlsym_as(tlListGet(pair, 0));
        tlset_add_(keys, name);
    }

    tlMap* map = tlmap_new(task, keys);
    for (int i = 0; i < size; i++) {
        tlList* pair = tlListAs(tlListGet(pairs, i));
        tlSym name = tlsym_as(tlListGet(pair, 0));
        tlValue v = tlListGet(pair, 1);
        tlmap_set_sym_(map, name, v);
    }
    return map;
}

tlValue tlmap_value_iter(tlMap* map, int i) {
    assert(i >= 0);
    if (i >= tlmap_size(map)) return null;
    return map->data[i];
}

void tlmap_value_iter_set_(tlMap* map, int i, tlValue v) {
    assert(i >= 0 && i < tlmap_size(map));
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

    tlSet* keys = tlset_new(task, size);
    va_start(ap, task);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlValue);
        tlset_add_(keys, tlSYM(n));
    }
    va_end(ap);

    tlMap* map = tlmap_new(task, keys);
    va_start(ap, task);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlValue v = va_arg(ap, tlValue); assert(v);
        tlmap_set_sym_(map, tlSYM(n), v);
    }
    va_end(ap);

    map->head.klass = tlValueObjectClass;
    return map;
}

tlMap* tlClassMapFrom(const char* n1, tlHostCb fn1, ...) {
    va_list ap;
    int size = 1;

    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlHostCb fn = va_arg(ap, tlHostCb); assert(fn);
        size++;
    }
    va_end(ap);

    tlSet* keys = tlset_new(null, size);
    tlset_add_(keys, tlSYM(n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        va_arg(ap, tlHostCb);
        tlset_add_(keys, tlSYM(n));
    }
    va_end(ap);

    tlMap* map = tlmap_new(null, keys);
    tlmap_set_sym_(map, tlSYM(n1), tlFUN(fn1, n1));
    va_start(ap, fn1);
    while (true) {
        const char* n = va_arg(ap, const char*); if (!n) break;
        tlHostCb fn = va_arg(ap, tlHostCb);
        tlmap_set_sym_(map, tlSYM(n), tlFUN(fn, n));
    }
    va_end(ap);

    return map;
}

// called when map literals contain lookups or expressions to evaluate
static tlValue _map_clone(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsAt(args, 0));
    if (!map) TL_THROW("Expected a map");
    int size = tlmap_size(map);
    map = tlAllocClone(task, map, sizeof(tlMap), size);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!map->data[i] || map->data[i] == tlUndefined) {
            map->data[i] = tlArgsAt(args, argc++);
        }
    }
    assert(argc == tlArgsSize(args));
    return map;
}
static tlValue _map_dump(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsAt(args, 0));
    if (!map) TL_THROW("Expected a map");
    tlmap_dump(map);
    return tlNull;
}
static tlValue _map_is(tlTask* task, tlArgs* args) {
    return tlBOOL(tlMapIs(tlArgsAt(args, 0)));
}
static tlValue _object_is(tlTask* task, tlArgs* args) {
    tlValue map = tlMapCast(tlArgsAt(args, 0));
    if (!map) return tlFalse;
    if (tlflag_isset(map, TL_FLAG_ISOBJECT)) return tlTrue;
    return tlFalse;
}
static tlValue _object_from(tlTask* task, tlArgs* args) {
    tlValue map = tlMapCast(tlArgsAt(args, 0));
    if (!map) TL_THROW("Expected a map");
    if (tlflag_isset(map, TL_FLAG_ISOBJECT)) return map;
    map = TL_CLONE(map);
    tlflag_set(map, TL_FLAG_ISOBJECT);
    return map;
}
static tlValue _MapSize(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    return tlINT(tlmap_size(map));
}
static tlValue _MapGet(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlValue key = tlArgsAt(args, 0);
    if (!key) TL_THROW("Excpected a key");
    tlValue res = tlmap_get(task, map, key);
    if (!res) return tlUndefined;
    return res;
}
static tlValue _MapSet(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlValue key = tlArgsAt(args, 0);
    if (!key) TL_THROW("Expected a key");
    tlValue val = tlArgsAt(args, 1);
    if (!val || val == tlUndefined) val = tlNull;
    tlMap* nmap = tlmap_set(task, map, key, val);
    return nmap;
}
static tlValue _MapToObject(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlMap* nmap = tlmap_new(task, map->keys);
    for (int i = 0; i < tlmap_size(map); i++) nmap->data[i] = map->data[i];
    nmap->head.klass = tlValueObjectClass;
    return nmap;
}

const char* _MapToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Map@%p %d>", v, tlmap_size(tlMapAs(v))); return buf;
}
static tlClass _tlMapClass = {
    .name = "text",
    .toText = _MapToText,
};

const char* _ValueToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<ValueObject@%p %d>", v, tlmap_size(tlMapFromObjectAs(v))); return buf;
}
static tlValue _ValueReceive(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapFromObjectCast(tlArgsTarget(args));
    tlSym msg = tlArgsMsg(args);

    tlValue field = tlmap_get(task, map, msg);
    trace("VALUE SEND %p: %s -> %s", map, tl_str(msg), tl_str(field));
    if (!field) {
        do {
            tlMap* klass = tlmap_get(task, map, s_class);
            trace("CLASS: %s", tl_str(klass));
            if (!klass) break;
            field = tlmap_get(task, klass, msg);
            if (field) {
                if (!tlcallable_is(field)) return field;
                return tlEvalArgsFn(task, args, field);
            }
        } while (true);
        return tlUndefined;
    }
    if (!tlcallable_is(field)) return field;
    return tlEvalArgsFn(task, args, field);
}
static tlClass _tlValueObjectClass = {
    .name = "ValueObject",
    .toText = _ValueToText,
    .send = _ValueReceive,
};

static void map_init() {
    _tlMapClass.map = tlClassMapFrom(
            "size", _MapSize,
            "get", _MapGet,
            "set", _MapSet,
            "toObject", _MapToObject,
            null
    );
    _tl_emptyMap = tlmap_new(null, null);
}

