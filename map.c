// a map implementation

#include "trace-on.h"

static tMap* v_map_empty;

static void map_init() {
    v_map_empty = tmap_as(v_list_empty);
}

#define HASKEYS(m) ((m->head.flags & TKEYS) == TKEYS)
#define HASLIST(m) ((m->head.flags & TLIST) == TLIST)
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
    }
}

tMap* tmap_new(tTask* task, tList* keys) {
    tMap* map = task_alloc(task, TMap, tlist_size(keys) + 1);
    map->head.flags |= TKEYS;
    _KEYS(map) = keys;
    return map;
}

tMap* tmap_from1(tTask* task, tValue key, tValue v) {
    if (key == tZero) return tlist_from1(task, v);
    tMap* map = task_alloc(task, TMap, 2);
    map->head.flags |= TKEYS;
    _KEYS(map) = tlist_from1(task, key);
    map->data[_OFFSET(map)] = v;
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

    tMap* map = task_alloc(task, TMap, (list?1:0)+(keys?1:0)+keysize);
    if (list) {
        map->head.flags |= TLIST;
        _LIST(map) = list;
        assert(keys);
    }
    if (keys) {
        map->head.flags |= TKEYS;
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
        }
    }

    if (list) assert(_LIST(map) == list);
    if (keys) assert(_KEYS(map) == keys);
    tmap_dump(map);
    return map;
}

int tmap_size(tMap* map) {
    if (HASKEYS(map)) {
        if (HASLIST(map)) {
            return tlist_size(_KEYS(map)) + tlist_size(_LIST(map));
        }
        return tlist_size(_KEYS(map));
    }
    return tlist_size(map);
}

int tmap_is_empty(tMap* map) { return tmap_size(map) == 0; }

tValue tmap_get_int(tMap* map, int key) {
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
    }
    return tUndefined;
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
    return tUndefined;
}

