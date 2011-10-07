// a map implementation

#include "trace-off.h"

static tlClass _tlMapClass;
tlClass* tlMapClass = &_tlMapClass;

static tlMap* _tl_emptyMap;

struct tlMap {
    tlHead head;
    tlSet* keys;
    tlValue data[];
};

tlMap* tlmap_empty() { return _tl_emptyMap; }

tlMap* tlmap_new(tlTask* task, tlSet* keys) {
    if (!keys) keys = v_set_empty;
    tlMap* map = tlAllocWithFields(task, sizeof(tlMap), tlMapClass, tlset_size(keys));
    map->keys = keys;
    assert(tlmap_size(map) == tlset_size(keys));
    return map;
}
int tlmap_size(tlMap* map) {
    assert(tlMapIs(map));
    return map->head.size;
}
tlSet* tlmap_keyset(tlMap* map) {
    return map->keys;
}
void tlmap_dump(tlMap* map) {
    print("---- MAP DUMP @ %p ----", map);
    for (int i = 0; i < tlmap_size(map); i++) {
        print("%d %s: %s", i, tl_str(map->data[i]), tl_str(map->keys->data[i]));
    }
    print("----");
}

tlValue tlmap_get(tlTask* task, tlMap* map, tlValue key) {
    assert(tlMapIs(map));
    int at = tlset_indexof(map->keys, key);
    if (at < 0) return null;
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
    assert(tlMapIs(map));
    int at = tlset_indexof(map->keys, key);
    if (at < 0) return null;
    assert(at < tlmap_size(map));
    trace("keys get: %s = %s", tl_str(key), tl_str(map->data[at]));
    return map->data[at];
}
void tlmap_set_sym_(tlMap* map, tlSym key, tlValue v) {
    assert(tlMapIs(map));
    int at = tlset_indexof(map->keys, key);
    assert(at >= 0 && at < tlmap_size(map));
    trace("keys set_: %s = %s", tl_str(key), tl_str(map->data[at]));
    map->data[at] = v;
}

tlMap* tlmap_from_pairs(tlTask* task, tlList* pairs) {
    int size = tllist_size(pairs);
    trace("%d", size);

    tlSet* keys = tlset_new(task, size);
    for (int i = 0; i < size; i++) {
        tlList* pair = tllist_as(tllist_get(pairs, i));
        tlSym name = tlsym_as(tllist_get(pair, 0));
        tlset_add_(keys, name);
    }

    tlMap* map = tlmap_new(task, keys);
    for (int i = 0; i < size; i++) {
        tlList* pair = tllist_as(tllist_get(pairs, i));
        tlSym name = tlsym_as(tllist_get(pair, 0));
        tlValue v = tllist_get(pair, 1);
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
static tlPause* _map_clone(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlargs_get(args, 0));
    if (!map) TL_THROW("Expected a map");
    int size = tlmap_size(map);
    map = TL_CLONE(map);
    int argc = 1;
    for (int i = 0; i < size; i++) {
        if (!map->data[i]) map->data[i] = tlargs_get(args, argc++);
    }
    TL_RETURN(map);
}
static tlPause* _map_dump(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlargs_get(args, 0));
    if (!map) TL_THROW("Expected a map");
    tlmap_dump(map);
    TL_RETURN(tlNull);
}
static tlPause* _map_is(tlTask* task, tlArgs* args) {
    TL_RETURN(tlBOOL(tlMapIs(tlargs_get(args, 0))));
}
static tlPause* _object_is(tlTask* task, tlArgs* args) {
    tlValue map = tlMapCast(tlargs_get(args, 0));
    if (!map) TL_RETURN(tlFalse);
    if (tlflag_isset(map, TL_FLAG_ISOBJECT)) TL_RETURN(tlTrue);
    TL_RETURN(tlFalse);
}
static tlPause* _object_from(tlTask* task, tlArgs* args) {
    tlValue map = tlMapCast(tlargs_get(args, 0));
    if (!map) TL_THROW("Expected a map");
    if (tlflag_isset(map, TL_FLAG_ISOBJECT)) TL_RETURN(map);
    map = TL_CLONE(map);
    tlflag_set(map, TL_FLAG_ISOBJECT);
    TL_RETURN(map);
}
static tlPause* _MapSize(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    TL_RETURN(tlINT(tlmap_size(map)));
}
static tlPause* _MapGet(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlValue key = tlArgsAt(args, 0);
    if (!key) TL_THROW("Excpected a key");
    tlValue res = tlmap_get(task, map, key);
    if (!res) TL_RETURN(tlUndefined);
    TL_RETURN(res);
}
static tlPause* _MapSet(tlTask* task, tlArgs* args) {
    tlMap* map = tlMapCast(tlArgsTarget(args));
    if (!map) TL_THROW("Expected a map");
    tlValue key = tlArgsAt(args, 0);
    if (!key) TL_THROW("Expected a key");
    tlValue val = tlArgsAt(args, 1);
    if (!val || val == tlUndefined) val = tlNull;
    tlMap* nmap = tlmap_set(task, map, key, val);
    TL_RETURN(nmap);
}

/*
static const tlHostCbs __map_hostcbs[] = {
    { "_map_clone", _map_clone },
    { "_map_dump",  _map_dump },
    { "_map_is",    _map_is },
    { "_object_is", _object_is },
    { "_object_from", _object_from },
    { "_map_size",  _map_size },
    { "_map_get",   _map_get },
    { "_map_set",   _map_set },
    { 0, 0 }
};
*/

const char* _MapToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "<Map@%p %d>", v, tlmap_size(tlMapAs(v))); return buf;
}

static tlClass _tlMapClass = {
    .name = "text",
    .toText = _MapToText,
};

static void map_init() {
    _tlMapClass.map = tlClassMapFrom(
            "size", _MapSize,
            "get", _MapGet,
            "set", _MapSet,
            null
    );
    _tl_emptyMap = tlmap_new(null, null);
}

