// a map implementation

#include "trace-on.h"

static tMap* v_map_empty;

static void map_init() {
    v_map_empty = tmap_as(v_list_empty);
}

#define HASKEYS(m) ((m->head.flags & T_FLAG_HASKEYS) > 0)
#define HASLIST(m) ((m->head.flags & T_FLAG_HASLIST) > 0)
#define _KEYS(m) m->data[0]
#define _LIST(m) m->data[1]
#define _OFFSET(m) (HASKEYS(m) + HASLIST(m))

void tmap_dump(tMap* map) {
    print("---- MAP DUMP @ %p ----", map);
    if (HASLIST(map)) {
        tList* list = _LIST(map);
        for (int i = 0; i < tlist_size(list); i++) {
            print("%d: %s", i, t_str(tlist_get(list, i)));
        }
    }
    if (HASKEYS(map)) {
        tList* keys = _KEYS(map);
        for (int i = 0; i < tlist_size(keys); i++) {
            tValue key = tlist_get(keys, i);
            if (key) {
                tValue v = map->data[_OFFSET(map) + i];
                print("%s: %s", t_str(key), t_str(v));
            }
        }
    } else {
        for (int i = 0; i < map->head.size; i++) {
            print("%d: %s", i, t_str(map->data[i]));
        }
    }
}

tMap* tmap_new(tTask* task, int size) {
    return task_alloc(task, TMap, size);
}
tMap* tmap_copy(tTask* task, tMap* o) {
    assert(tmap_is(o));
    tMap* map = task_alloc(task, TMap, o->head.size);
    if (HASKEYS(o)) {
        map->head.flags |= T_FLAG_HASKEYS;
        _KEYS(map) = _KEYS(o);
    }
    if (HASLIST(o)) {
        assert(HASKEYS(map));
        map->head.flags |= T_FLAG_HASLIST;
        _LIST(map) = tlist_new(task, tlist_size(_LIST(o)));
    }
    return map;
}

tMap* tmap_new_keys(tTask* task, tList* keys, int size) {
    trace("new map keys: %s %d", t_str(keys), size);
    tMap* map;
    if (keys) map = tmap_copy(task, tlist_get(keys, size));
    else map = tmap_new(task, size);
    return map;
}
void tmap_set_key_(tMap* map, tValue key, int at, tValue v) {
    trace("SET KEY: %d %d -- %d", HASLIST(map), HASKEYS(map), at);
    if (key) {
        assert(HASLIST(map));
        if (tint_is(key)) {
            tmap_set_int_(map, t_int(key), v);
        } else {
            tmap_set_sym_(map, tsym_as(key), v);
        }
    } else {
        assert(!(HASLIST(map) && HASKEYS(map)));
        assert(!(at < 0 && at >= map->head.size));
        map->data[at] = v;
    }
}

tMap* tmap_from1(tTask* task, tValue key, tValue v) {
    if (key == tZero) return tlist_from1(task, v);
    tMap* map = task_alloc(task, TMap, 2);
    map->head.flags |= T_FLAG_HASKEYS;
    _KEYS(map) = tlist_from1(task, key);
    map->data[_OFFSET(map)] = v;
    return map;
}

tMap* tmap_from_pairs(tTask* task, tList* pairs) {
    int size = tlist_size(pairs);
    tList* keys = tlist_new(task, size);
    for (int i = 0; i < size; i++) {
        tList* pair = tlist_as(tlist_get(pairs, i));
        tSym name = tsym_as(tlist_get(pair, 0));
        set_add_(keys, name);
    }
    tMap* map = tmap_new(task, size + 1);
    map->head.flags |= T_FLAG_HASKEYS;
    _KEYS(map) = keys;
    for (int i = 0; i < size; i++) {
        tList* pair = tlist_as(tlist_get(pairs, i));
        tSym name = tsym_as(tlist_get(pair, 0));
        tValue v = tlist_get(pair, 1);
        tmap_set_sym_(map, name, v);
    }
    return map;
}
tMap* tmap_from_list(tTask* task, tList* pairs) {
    int size = tlist_size(pairs);
    assert(size % 2 == 0);
    size = size / 2;

    tList* list = null;
    tList* keys = null;

    int intsize = 0;
    for (int i = 0; i < size; i++) if (tlist_get(pairs, i * 2) == tNull) intsize++;
    if (intsize) list = tlist_new(task, intsize);

    int keysize = size - intsize;
    if (keysize) {
        keys = tlist_new(task, keysize);
        for (int i = 0; i < size; i++) {
            tValue key = tlist_get(pairs, i * 2);
            if (key == tNull) continue;
            set_add_(keys, key);
        }
        int realkeysize = keysize;
        for (int i = keysize - 1; i >= 0; i--) {
            if (tlist_get(keys, i)) break;
            realkeysize--;
        }
        trace("keysize/realkeysize: %d/%d", keysize, realkeysize);
        if (keysize/(float)realkeysize > 1.3) {
            keys = tlist_copy(task, keys, realkeysize);
        }
        keysize = realkeysize;
    }
    trace("CONSTRUCT: %d, %d", intsize, keysize);

    if (!keys) {
        if (intsize == 0) return v_map_empty;
        for (int i = 0; i < size; i++) {
            tValue key = tlist_get(pairs, i * 2);
            assert(key == tNull);
            tValue v = tlist_get(pairs, i * 2 + 1);
            assert(v);
            tlist_set_(list, i, v);
        }
        list->head.type = TMap;
        assert(tmap_is(list));
        tmap_dump(list);
        return list;
    }

    tMap* map = task_alloc(task, TMap, (list?1:0)+(keys?1:0)+keysize);
    if (list) {
        map->head.flags |= T_FLAG_HASLIST;
        _LIST(map) = list;
        assert(keys);
    }
    if (keys) {
        map->head.flags |= T_FLAG_HASKEYS;
        _KEYS(map) = keys;
    }
    if (list) assert(_LIST(map) == list);
    if (keys) assert(_KEYS(map) == keys);

    int nextint = 0;
    for (int i = 0; i < size; i++) {
        tValue key = tlist_get(pairs, i * 2);
        tValue v = tlist_get(pairs, i * 2 + 1);
        if (key == tNull) {
            tlist_set_(_LIST(map), nextint, v); nextint++;
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

int tmap_size(tMap* map) {
    if (HASKEYS(map)) {
        if (HASLIST(map)) {
            return tlist_size(_KEYS(map)) + tlist_size(_LIST(map));
        }
        return tlist_size(_KEYS(map));
    }
    return map->head.size;
}

int tmap_is_empty(tMap* map) { return tmap_size(map) == 0; }

tValue tmap_value_iter(tMap* map, int i) {
    tmap_dump(map);
    assert(i >= 0);
    if (HASLIST(map)) {
        print("HASLIST: %d", i);
        tList* list = _LIST(map);
        if (i < tlist_size(list)) return tlist_get(list, i);
        i -= tlist_size(list);
    }
    print("OFFSET=%d at=%d size=%d", _OFFSET(map), i, map->head.size);
    if (i + _OFFSET(map) < map->head.size) {
        print("%s", t_str(tmap_get_int(map, i)));
        return map->data[_OFFSET(map) + i];
    }
    return null;
}

tValue tmap_get_int(tMap* map, int key) {
    trace("get: %p %d %d", map, key, map->head.size);
    if (HASLIST(map)) {
        if (!(key < 0 || key >= tlist_size(_LIST(map)))) {
            trace("list get: %d = %s", key, t_str(tlist_get(_LIST(map), key)));
            return tlist_get(_LIST(map), key);
        }
    }
    if (HASKEYS(map)) {
        int at = set_indexof(_KEYS(map), tINT(key));
        if (at >= 0) {
            trace("keys get: %d = %s", key, t_str(map->data[_OFFSET(map) + at]));
            return map->data[_OFFSET(map) + at];
        }
    } else {
        assert(_OFFSET(map) == 0);
        if (key < 0 || key >= map->head.size) return null;
        return map->data[key];
    }
    return null;
}

void tmap_set_int_(tMap* map, int key, tValue v) {
    if (HASLIST(map)) {
        if (!(key < 0 || key >= tlist_size(_LIST(map)))) {
            trace("list set: %d = %s", key, t_str(v));
            tlist_set_(_LIST(map), key, v);
            return;
        }
    }
    if (HASKEYS(map)) {
        int at = set_indexof(_KEYS(map), tINT(key));
        if (at >= 0) {
            trace("keys set: %d = %s", key, t_str(v));
            map->data[_OFFSET(map) + at] = v;
        }
    }
    // TODO just a list...
    assert(false);
}

tValue tmap_get_sym(tMap* map, tSym key) {
    if (HASKEYS(map)) {
        int at = set_indexof(_KEYS(map), key);
        if (at >= 0) {
            trace("keys get: %s = %s", t_str(key), t_str(map->data[_OFFSET(map) + at]));
            return map->data[_OFFSET(map) + at];
        }
    }
    return null;
}

void tmap_set_sym_(tMap* map, tSym key, tValue v) {
    trace("set sym: %s", t_str(key));
    assert(HASKEYS(map));
    int at = set_indexof(_KEYS(map), key);
    assert(at >= 0);
    map->data[_OFFSET(map) + at] = v;
}

tMap* tmap_set(tTask* task, tMap* map, tValue key, tValue v) {
    trace("set map: %s = %s", t_str(key), t_str(v));
    //if (tint_is(key)) return tmap_set_int(task, map, tint_as(key), v);

    tList* keys = null;
    int at = 0;
    if (HASKEYS(map)) {
        keys = _KEYS(map);
        at = set_indexof(keys, key);
        if (at >= 0) {
            tMap* nmap = tmap_copy(task, map);
            for (int i = 0; i < tlist_size(keys); i++) {
                nmap->data[_OFFSET(nmap) + i] = map->data[_OFFSET(map) + i];
            }
            nmap->data[_OFFSET(nmap) + at] = v;
            return nmap;
        }
        keys = tlist_copy(task, keys, tlist_size(keys) + 1);
        at = set_add_(keys, key);
    } else {
        keys = tlist_from1(task, key);
    }
    assert(!HASLIST(map));
    assert(keys);

    tMap* nmap = tmap_new(task, tlist_size(keys) + 1);
    nmap->head.flags |= T_FLAG_HASKEYS;
    _KEYS(nmap) = keys;
    int i;
    for (i = 0; i < at; i++) {
        nmap->data[_OFFSET(nmap) + i] = map->data[_OFFSET(map) + i];
    }
    nmap->data[_OFFSET(nmap) + at] = v; i++;
    for (; i < tlist_size(keys); i++) {
        nmap->data[_OFFSET(nmap) + i] = map->data[_OFFSET(map) + i - 1];
    }
    return nmap;
}

