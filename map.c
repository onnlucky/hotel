// an ordered multi map implementation
// TODO
// it is currently not *MULTI* map ...
//
// this is a very inefficient implementation
// * compare ints differently so we can find the largest/smallest quickly
// * allow number only maps to use a list
// * use a rope like thing, with 8 size order needs 4 bits per entry = 32 bits total
//
// implement addFirst/addLast list like behaviour and such...

#include "trace-on.h"

typedef struct tMap tMap;
struct tMap {
    tHead head;
    tList* keys;
    tList* order;
    tValue data[];
};
bool tmap_is(tValue v) { return t_type(v) == TMap; }
tMap* tmap_as(tValue v) { assert(tmap_is(v)); return (tMap*)v; }
tMap* tmap_cast(tValue v) { if (tmap_is(v)) return (tMap*)v; else return null; }

static tMap* t_map_empty;

int tmap_size(tMap* map) { return map->head.size; }

void map_dump(tMap* map) {
    print("*******************************");
    for (int i = 0; i < tmap_size(map); i++) {
        print("%d - %d = %s -> %s", i, t_int(map->order->data[i]),
                t_str(map->keys->data[i]), t_str(map->data[i]));
    }
    print("*******************************");
}


// ** set **

int set_indexof(tList* set, tValue key) {
    int size = tlist_size(set);
    if (key == null || size == 0) return -1;

    int min = 0, max = size - 1;
    do {
        int mid = (min + max) / 2;
        if (set->data[mid] < key) min = mid + 1; else max = mid;
    } while (min < max);
    if (min != max) return -1;
    if (set->data[min] != key) return -1;
    return min;
}

tList* set_add(tTask* task, tList* set, tValue key, int* at) {
    trace();
    *at = set_indexof(set, key);
    if (key == null || *at >= 0) return set;

    trace("adding key: %s", t_str(key));
    int size = tlist_size(set);
    tList* nset;
    if (task) nset = tlist_new(task, size + 1);
    else nset = tlist_new_global(size + 1);

    int i = 0;
    for (; i < size && set->data[i] < key; i++) nset->data[i] = set->data[i];
    *at = i;
    nset->data[i] = key;
    for (; i < size; i++) nset->data[i + 1] = set->data[i];

    return nset;
}


// ** map **

tMap* tmap_new_global(int size) {
    if (size == 0) return t_map_empty;
    tMap* map = global_alloc(TMap, size);
    return map;
}

tMap* tmap_new(tTask* task, int size) {
    if (size == 0) return t_map_empty;
    tMap* map = task_alloc(task, TMap, size);
    return map;
}

// get value by its key
tValue tmap_get(tMap* map, tValue key) {
    assert(tmap_is(map));
    int at = set_indexof(map->keys, key);
    if (at >= 0) {
        trace("get: %s = %s", t_str(key), t_str(map->data[at]));
        return map->data[at];
    }
    return tUndef;
}

void tmap_at(tMap* map, int at, tValue* key, tValue* val) {
    assert(tmap_is(map));
    assert(at >= 0 && at < tmap_size(map));
    int i = t_int(tlist_get(map->order, at));
    assert(i >= 0 && i < tmap_size(map));

    *key = map->keys->data[i];
    *val = map->data[i];
}

tInt tmap_nextint(tMap* map) {
    int max = 0;
    for (int i = 0; i < tlist_size(map->keys); i++) {
        tValue key = tlist_get(map->keys, i);
        if (tint_is(key)) {
            int n = t_int(key);
            if (n > max) max = n;
        }
    }
    return tINT(max + 1);
}

// set a value for a key, adding the key as the last element, if needed
tMap* tmap_set(tTask* task, tMap* map, tValue key, tValue v) {
    assert(tmap_is(map));
    if (key == tNull) key = tmap_nextint(map);

    int size = tmap_size(map);
    int at = -1;
    tList* keys = set_add(task, map->keys, key, &at);
    assert(at >= 0);
    assert(tlist_is(keys));

    tMap* nmap;
    if (task) nmap = tmap_new(task, tlist_size(keys));
    else nmap = tmap_new_global(tlist_size(keys));
    nmap->keys = keys;

    if (keys == map->keys) {
        assert(tmap_size(map) == tmap_size(nmap));
        nmap->order = map->order;
        for (int i = 0; i < size; i++) {
            nmap->data[i] = map->data[i];
        }
        nmap->data[at] = v;
        return nmap;
    }

    trace("growing map");
    assert(tmap_size(map) == tmap_size(nmap) - 1);

    // update the order of inserts
    nmap->order = tlist_copy(task, map->order, tmap_size(nmap));
    nmap->order->data[tmap_size(nmap) - 1] = tINT(at);
    for (int i = 0; i < tmap_size(nmap); i++) {
        int n = t_int(nmap->order->data[i]);
        if (n >= at) nmap->order->data[i] = tINT(n + 1);
    }
    nmap->order->data[tmap_size(nmap) - 1] = tINT(at);

    int i = 0;
    for (; i < at; i++) {
        nmap->data[i] = map->data[i];
    }
    nmap->data[i] = v;
    for (; i < size; i++) {
        nmap->data[i + 1] = map->data[i];
    }

    trace("SET %d: %s = %s", at, t_str(key), t_str(v));
    map_dump(map);
    return nmap;
}

void map_init() {
    t_map_empty = global_alloc(TMap, 0);
    t_map_empty->keys = t_list_empty;
    t_map_empty->order = t_list_empty;
}

void map_test() {
    tMap* map = t_map_empty;
    map = tmap_set(null, map, tSYM("foo"), tTEXT("foo entry"));
    map = tmap_set(null, map, tNull, tTEXT("one"));
    map = tmap_set(null, map, tNull, tTEXT("two"));
    map = tmap_set(null, map, tTEXT("oops"), tTEXT("oops entry"));

    print("test: foo = %s", t_str(tmap_get(map, tSYM("foo"))));
    for (int i = 0; i < tmap_size(map); i++) {
        tValue key;
        tValue val;
        tmap_at(map, i, &key, &val);
        print("%d: %s = %s", i, t_str(key), t_str(val));
    }
    return;
}

