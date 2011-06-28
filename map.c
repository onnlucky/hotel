// a map implementation

#include "trace-off.h"

static tlMap* v_map_empty;

static void map_init() {
    v_map_empty = tlmap_as(v_list_empty);
}

#define HASKEYS(m) ((m->head.flags & TL_FLAG_HASKEYS) > 0)
#define HASLIST(m) ((m->head.flags & TL_FLAG_HASLIST) > 0)
#define _KEYS(m) m->data[0]
#define _LIST(m) m->data[1]
#define _OFFSET(m) (HASKEYS(m) + HASLIST(m))

void tlmap_dump(tlMap* map) {
    print("---- MAP DUMP @ %p ----", map);
    if (HASLIST(map)) {
        tlList* list = _LIST(map);
        for (int i = 0; i < tllist_size(list); i++) {
            print("%d: %s", i, tl_str(tllist_get(list, i)));
        }
    }
    if (HASKEYS(map)) {
        tlList* keys = _KEYS(map);
        for (int i = 0; i < tllist_size(keys); i++) {
            tlValue key = tllist_get(keys, i);
            if (key) {
                tlValue v = map->data[_OFFSET(map) + i];
                print("%s: %s", tl_str(key), tl_str(v));
            }
        }
    } else {
        for (int i = 0; i < map->head.size; i++) {
            print("%d: %s", i, tl_str(map->data[i]));
        }
    }
}

tlMap* tlmap_new(tlTask* task, int size) {
    return task_alloc(task, TLMap, size);
}
tlMap* tlmap_copy_empty(tlTask* task, tlMap* o) {
    assert(tlmap_is(o));
    tlMap* map = task_alloc(task, TLMap, o->head.size);
    if (HASKEYS(o)) {
        map->head.flags |= TL_FLAG_HASKEYS;
        _KEYS(map) = _KEYS(o);
    }
    if (HASLIST(o)) {
        assert(HASKEYS(map));
        map->head.flags |= TL_FLAG_HASLIST;
        _LIST(map) = tllist_new(task, tllist_size(_LIST(o)));
    }
    return map;
}
tlMap* tlmap_copy(tlTask* task, tlMap* o) {
    tlMap* map = tlmap_copy_empty(task, o);
    if (HASLIST(map)) {
        assert(HASLIST(o));
        _LIST(map) = tllist_copy(task, _LIST(o), -1);
    }
    for (int i = _OFFSET(map); i < map->head.size; i++) {
        map->data[i] = o->data[i];
    }
    return map;
}

tlMap* tlmap_new_keys(tlTask* task, tlList* keys, int size) {
    trace("new map keys: %s %d", tl_str(keys), size);
    tlMap* map;
    if (keys) map = tlmap_copy_empty(task, tllist_get(keys, size));
    else map = tlmap_new(task, size);
    return map;
}
void tlmap_set_key_(tlMap* map, tlValue key, int at, tlValue v) {
    trace("SET KEY: %d %d -- %d", HASLIST(map), HASKEYS(map), at);
    if (key) {
        assert(HASLIST(map));
        if (tlint_is(key)) {
            tlmap_set_int_(map, tl_int(key), v);
        } else {
            tlmap_set_sym_(map, tlsym_as(key), v);
        }
    } else {
        assert(!(HASLIST(map) && HASKEYS(map)));
        assert(!(at < 0 && at >= map->head.size));
        map->data[at] = v;
    }
}

tlMap* tlmap_from1(tlTask* task, tlValue key, tlValue v) {
    if (key == tlZero) return tllist_from1(task, v);
    tlMap* map = task_alloc(task, TLMap, 2);
    map->head.flags |= TL_FLAG_HASKEYS;
    _KEYS(map) = tllist_from1(task, key);
    map->data[_OFFSET(map)] = v;
    return map;
}

tlMap* tlmap_from_pairs(tlTask* task, tlList* pairs) {
    int size = tllist_size(pairs);
    tlList* keys = tllist_new(task, size);
    for (int i = 0; i < size; i++) {
        tlList* pair = tllist_as(tllist_get(pairs, i));
        tlSym name = tlsym_as(tllist_get(pair, 0));
        set_add_(keys, name);
    }
    tlMap* map = tlmap_new(task, size + 1);
    map->head.flags |= TL_FLAG_HASKEYS;
    _KEYS(map) = keys;
    for (int i = 0; i < size; i++) {
        tlList* pair = tllist_as(tllist_get(pairs, i));
        tlSym name = tlsym_as(tllist_get(pair, 0));
        tlValue v = tllist_get(pair, 1);
        tlmap_set_sym_(map, name, v);
    }
    return map;
}
tlMap* tlmap_from_list(tlTask* task, tlList* pairs) {
    int size = tllist_size(pairs);
    assert(size % 2 == 0);
    size = size / 2;

    tlList* list = null;
    tlList* keys = null;

    int intsize = 0;
    for (int i = 0; i < size; i++) if (tllist_get(pairs, i * 2) == tlNull) intsize++;
    if (intsize) list = tllist_new(task, intsize);

    int keysize = size - intsize;
    if (keysize) {
        keys = tllist_new(task, keysize);
        for (int i = 0; i < size; i++) {
            tlValue key = tllist_get(pairs, i * 2);
            if (key == tlNull) continue;
            set_add_(keys, key);
        }
        int realkeysize = keysize;
        for (int i = keysize - 1; i >= 0; i--) {
            if (tllist_get(keys, i)) break;
            realkeysize--;
        }
        trace("keysize/realkeysize: %d/%d", keysize, realkeysize);
        if (keysize/(float)realkeysize > 1.3) {
            keys = tllist_copy(task, keys, realkeysize);
        }
        keysize = realkeysize;
    }
    trace("CONSTRUCT: %d, %d", intsize, keysize);

    if (!keys) {
        if (intsize == 0) return v_map_empty;
        for (int i = 0; i < size; i++) {
            tlValue key = tllist_get(pairs, i * 2);
            assert(key == tlNull);
            tlValue v = tllist_get(pairs, i * 2 + 1);
            assert(v);
            tllist_set_(list, i, v);
        }
        list->head.type = TLMap;
        assert(tlmap_is(list));
        return list;
    }

    tlMap* map = task_alloc(task, TLMap, (list?1:0)+(keys?1:0)+keysize);
    if (list) {
        map->head.flags |= TL_FLAG_HASLIST;
        _LIST(map) = list;
        assert(keys);
    }
    if (keys) {
        map->head.flags |= TL_FLAG_HASKEYS;
        _KEYS(map) = keys;
    }
    if (list) assert(_LIST(map) == list);
    if (keys) assert(_KEYS(map) == keys);

    int nextint = 0;
    for (int i = 0; i < size; i++) {
        tlValue key = tllist_get(pairs, i * 2);
        tlValue v = tllist_get(pairs, i * 2 + 1);
        if (key == tlNull) {
            tllist_set_(_LIST(map), nextint, v); nextint++;
        } else {
            int at = set_indexof(keys, key);
            assert(at >= 0);
            map->data[_OFFSET(map) + at] = v;
            if (list) assert(_LIST(map) == list);
            if (keys) assert(_KEYS(map) == keys);
        }
    }

    if (list) assert(_LIST(map) == list);
    if (keys) assert(_KEYS(map) == keys);
    return map;
}

int tlmap_size(tlMap* map) {
    if (HASKEYS(map)) {
        if (HASLIST(map)) {
            return tllist_size(_KEYS(map)) + tllist_size(_LIST(map));
        }
        return tllist_size(_KEYS(map));
    }
    return map->head.size;
}

int tlmap_is_empty(tlMap* map) { return tlmap_size(map) == 0; }

tlValue tlmap_value_iter(tlMap* map, int i) {
    assert(i >= 0);
    if (HASLIST(map)) {
        tlList* list = _LIST(map);
        if (i < tllist_size(list)) return tllist_get(list, i);
        i -= tllist_size(list);
    }
    if (i + _OFFSET(map) < map->head.size) {
        return map->data[_OFFSET(map) + i];
    }
    return null;
}

tlMap* tlmap_value_iter_set_(tlMap* map, int i, tlValue v) {
    assert(i >= 0);
    if (HASLIST(map)) {
        tlList* list = _LIST(map);
        if (i < tllist_size(list)) {
            tllist_set_(list, i, v);
            return map;
        }
        i -= tllist_size(list);
    }
    if (i + _OFFSET(map) < map->head.size) {
        map->data[_OFFSET(map) + i] = v;
        return map;
    }
    assert(false);
    return map;
}

tlValue tlmap_get_int(tlMap* map, int key) {
    trace("get: %p %d %d", map, key, map->head.size);
    if (HASLIST(map)) {
        if (!(key < 0 || key >= tllist_size(_LIST(map)))) {
            trace("list get: %d = %s", key, tl_str(tllist_get(_LIST(map), key)));
            return tllist_get(_LIST(map), key);
        }
    }
    if (HASKEYS(map)) {
        int at = set_indexof(_KEYS(map), tlINT(key));
        if (at >= 0) {
            trace("keys get: %d = %s", key, tl_str(map->data[_OFFSET(map) + at]));
            return map->data[_OFFSET(map) + at];
        }
    } else {
        assert(_OFFSET(map) == 0);
        if (key < 0 || key >= map->head.size) return null;
        return map->data[key];
    }
    return null;
}

void tlmap_set_int_(tlMap* map, int key, tlValue v) {
    if (HASLIST(map)) {
        if (!(key < 0 || key >= tllist_size(_LIST(map)))) {
            trace("list set: %d = %s", key, tl_str(v));
            tllist_set_(_LIST(map), key, v);
            return;
        }
    }
    if (HASKEYS(map)) {
        int at = set_indexof(_KEYS(map), tlINT(key));
        if (at >= 0) {
            trace("keys set: %d = %s", key, tl_str(v));
            map->data[_OFFSET(map) + at] = v;
        }
    }
    // TODO just a list...
    assert(false);
}

tlValue tlmap_get_sym(tlMap* map, tlSym key) {
    if (HASKEYS(map)) {
        int at = set_indexof(_KEYS(map), key);
        if (at >= 0) {
            trace("keys get: %s = %s", tl_str(key), tl_str(map->data[_OFFSET(map) + at]));
            return map->data[_OFFSET(map) + at];
        }
    }
    return null;
}

void tlmap_set_sym_(tlMap* map, tlSym key, tlValue v) {
    trace("set sym: %s", tl_str(key));
    assert(HASKEYS(map));
    int at = set_indexof(_KEYS(map), key);
    assert(at >= 0);
    map->data[_OFFSET(map) + at] = v;
}

tlValue tlmap_get(tlTask* task, tlMap* map, tlValue key) {
    if (tlint_is(key)) return tlmap_get_int(map, tl_int(key));
    if (tlsym_is(key)) return tlmap_get_sym(map, tlsym_as(key));
    fatal("not implemented");
    return tlNull;
}

tlMap* tlmap_set(tlTask* task, tlMap* map, tlValue key, tlValue v) {
    trace("set map: %s = %s", tl_str(key), tl_str(v));
    //if (tlint_is(key)) return tlmap_set_int(task, map, tlint_as(key), v);

    tlList* keys = null;
    int at = 0;
    if (HASKEYS(map)) {
        keys = _KEYS(map);
        at = set_indexof(keys, key);
        if (at >= 0) {
            tlMap* nmap = tlmap_copy(task, map);
            nmap->data[_OFFSET(nmap) + at] = v;
            return nmap;
        }
        keys = tllist_copy(task, keys, tllist_size(keys) + 1);
        at = set_add_(keys, key);
    } else {
        keys = tllist_from1(task, key);
    }
    assert(!HASLIST(map));
    assert(keys);

    tlMap* nmap = tlmap_new(task, tllist_size(keys) + 1);
    nmap->head.flags |= TL_FLAG_HASKEYS;
    _KEYS(nmap) = keys;
    int i;
    for (i = 0; i < at; i++) {
        nmap->data[_OFFSET(nmap) + i] = map->data[_OFFSET(map) + i];
    }
    nmap->data[_OFFSET(nmap) + at] = v; i++;
    for (; i < tllist_size(keys); i++) {
        nmap->data[_OFFSET(nmap) + i] = map->data[_OFFSET(map) + i - 1];
    }
    return nmap;
}

