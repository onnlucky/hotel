// a mutable list implementation
// manually inserting things at > size, will fill array with null's (not undefined values!)

#include "trace-off.h"

static tlKind _tlArrayKind = { .name = "Array", .locked = true };
tlKind* tlArrayKind;

struct tlArray {
    tlLock lock;
    intptr_t size;
    intptr_t alloc;
    tlHandle* data;
};

tlArray* tlArrayNew() {
    tlArray* array = tlAlloc(tlArrayKind, sizeof(tlArray));
    return array;
}

int tlArraySize(tlArray* array) {
    return array->size;
}

tlArray* tlArraySub(tlArray* array, int offset, int len) {
    if (len == 0) return tlArrayNew();

    assert(len > 0);
    assert(offset < tlArraySize(array));
    assert(offset + len <= tlArraySize(array));

    tlArray* narray = tlArrayNew();
    for (int i = 0; i < len; i++) tlArrayAdd(narray, array->data[offset + i]);
    return narray;
}

tlArray* tlArrayClear(tlArray* array) {
    array->size = 0;
    return array;
}

tlArray* tlArrayAdd(tlArray* array, tlHandle v) {
    trace("add: %s.add %s -- %zd -- %zd", tl_str(array), tl_str(v), array->size, array->alloc);
    if (array->size == array->alloc) {
        if (!array->alloc) array->alloc = 8; else array->alloc *= 2;
        array->data = realloc(array->data, array->alloc * sizeof(tlHandle));
        assert(array->data);
    }
    array->data[array->size++] = v;
    return array;
}
tlHandle tlArrayPop(tlArray* array) {
    if (array->size == 0) return null;
    array->size--;
    tlHandle v = array->data[array->size];
    array->data[array->size] = 0;
    return (v)?v : tlNull;
}
tlHandle tlArrayRemove(tlArray* array, int i) {
    assert(i >= 0);
    if (i >= array->size) return null;
    tlHandle v = array->data[i];

    trace("remove: %d", i);
    memmove(array->data + i, array->data + i + 1, (array->size - i) * sizeof(tlHandle));
    array->size -= 1;

    return v;
}
tlArray* tlArrayInsert(tlArray* array, int i, tlHandle v) {
    assert(i >= 0);
    assert(i < 1000000); // TODO for now :)
    if (i == array->size) return tlArrayAdd(array, v);

    int nsize;
    if (i > array->size) {
        nsize = i + 1;
    } else {
        nsize = array->size + 1;
    }
    trace("insert: %d -- %zd nsize: %d -- %s", i, array->size, nsize, tl_str(v));
    if (nsize > array->alloc) {
        if (!array->alloc) array->alloc = 8; else array->alloc *= 2;
        while (nsize > array->alloc) array->alloc *= 2;
        array->data = realloc(array->data, array->alloc * sizeof(tlHandle));
        assert(array->data);
    }

    if (nsize - array->size > 0) {
        trace("clearing: %zd - %zd", array->size, nsize - array->size);
        memset(array->data + array->size, 0, (nsize - array->size) * sizeof(tlHandle));
    }
    if (array->size - i > 0) {
        trace("moving: %d - %zd", i + 1, array->size - i);
        memmove(array->data + i + 1, array->data + i, (array->size - i) * sizeof(tlHandle));
    }
    array->data[i] = v;
    array->size = nsize;

    return array;
}
tlHandle tlArrayGet(tlArray* array, int i) {
    if (i < 0) return null;
    if (i >= array->size) return null;
    tlHandle v = array->data[i];
    return (v)?v : tlNull;
}
tlArray* tlArraySet(tlArray* array, int i, tlHandle v) {
    assert(i >= 0);
    if (i == array->size) return tlArrayAdd(array, v);
    if (i > array->size) return tlArrayInsert(array, i, v);
    array->data[i] = v;
    return array;
}
tlList* tlArrayToList(tlArray* array) {
    tlList* list = tlListNew(array->size);
    for (int i = 0; i < array->size; i++) {
        tlHandle v = tlArrayGet(array, i);
        if (!v) v = tlNull;
        tlListSet_(list, i, v);
    }
    return list;
}

INTERNAL tlHandle _Array_new(tlArgs* args) {
    tlArray* array = tlArrayNew();
    for (int i = 0; i < 1000; i++) {
        tlHandle h = tlArgsGet(args, i);
        if (!h) return array;

        if (tlListIs(h)) {
            tlList* list = tlListAs(h);
            for (int i = 0; i < tlListSize(list); i++) {
                tlArrayAdd(array, tlListGet(list, i));
            }
            continue;
        }
        if (tlArrayIs(h)) {
            tlArray* from = tlArrayAs(h);
            for (int i = 0; i < tlArraySize(from); i++) {
                tlArrayAdd(array, tlArrayGet(from, i));
            }
            continue;
        }
        TL_THROW("expect list or array");
    }
    return array;
}
INTERNAL tlHandle _array_size(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    return tlINT(tlArraySize(array));
}
INTERNAL tlHandle _array_clear(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    return tlArrayClear(array);
}
INTERNAL tlHandle _array_toList(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    return tlArrayToList(array);
}
INTERNAL tlHandle _array_get(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    if (!tlIntIs(tlArgsGet(args, 0))) TL_THROW("require an index");
    int at = at_offset(tlArgsGet(args, 0), tlArraySize(array));
    return tlMAYBE(tlArrayGet(array, at));
}
INTERNAL tlHandle _array_set(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    if (!tlIntIs(tlArgsGet(args, 0))) TL_THROW("require an index");
    int at = at_offset_raw(tlArgsGet(args, 0));
    if (at < 0) at = at_offset(tlArgsGet(args, 0), tlArraySize(array));
    if (at < 0) TL_THROW("index must be >= 1");
    return tlArraySet(array, at, tlArgsGet(args, 1));
}
INTERNAL tlHandle _array_add(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    tlHandle v = tlNull;
    for (int i = 0; i < tlArgsSize(args); i++) {
        v = tlArgsGet(args, i);
        tlArrayAdd(array, v);
    }
    return tlResultFrom(v, tlINT(array->size), null);
}
INTERNAL tlHandle _array_cat(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlHandle v = tlArgsGet(args, i);
        if (tlListIs(v)) {
            tlList* other = tlListAs(v);
            for (int j = 0; j < tlListSize(other); j++) tlArrayAdd(array, tlListGet(other, j));
        } else if (tlArrayIs(v)) {
            tlArray* other = tlArrayAs(v);
            for (int j = 0; j < tlArraySize(other); j++) tlArrayAdd(array, tlArrayGet(other, j));
        } else {
            TL_THROW("expected an Array or List");
        }
    }
    return array;
}
INTERNAL tlHandle _array_pop(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    return tlMAYBE(tlArrayPop(array));
}
INTERNAL tlHandle _array_insert(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    if (!tlIntIs(tlArgsGet(args, 0))) TL_THROW("require an index");
    int at = at_offset_raw(tlArgsGet(args, 0));
    if (at < 0) at = at_offset(tlArgsGet(args, 0), tlArraySize(array));
    if (at < 0) TL_THROW("index must be >= 1");
    return tlArrayInsert(array, at, tlArgsGet(args, 1));
}
INTERNAL tlHandle _array_remove(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    if (!tlIntIs(tlArgsGet(args, 0))) TL_THROW("require an index");
    int at = at_offset(tlArgsGet(args, 0), tlArraySize(array));
    if (at < 0) TL_THROW("index must be >= 1");
    return tlMAYBE(tlArrayRemove(array, at));
}
INTERNAL tlHandle _array_splice(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));
    int from = at_offset_min(tlArgsGet(args, 0), tlArraySize(array));
    if (from < 0) TL_THROW("require an index from");
    int to = at_offset_max(tlArgsGet(args, 1), tlArraySize(array));
    if (to < 0) TL_THROW("require an index to");

    // swap if needed
    if (from > to) { int tmp = to; to = from; from = tmp; }

    for (int i = 0; i < to - from; i++) {
        tlArrayRemove(array, from);
    }

    int at = from - 1;
    for (int i = 2; i < tlArgsSize(args); i++) {
        tlHandle v = tlArgsGet(args, i);
        if (tlListIs(v)) {
            tlList* other = tlListAs(v);
            for (int j = 0; j < tlListSize(other); j++) {
                tlArrayInsert(array, ++at, tlListGet(other, j));
            }
        } else if (tlArrayIs(v)) {
            tlArray* other = tlArrayAs(v);
            for (int j = 0; j < tlArraySize(other); j++) {
                tlArrayInsert(array, ++at, tlArrayGet(other, j));
            }
        } else {
            TL_THROW("expected an Array or List");
        }
    }
    return array;
}

/// slice(left, right): return a new list, from the range between left and right
INTERNAL tlHandle _array_slice(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));

    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), -1);

    int offset;
    int len = sub_offset(first, last, tlArraySize(array), &offset);
    return tlArraySub(array, offset, len);
}

// TODO by lack of better name; akin to int.toChar ...
// TODO taken from list.c ... duplicate code ... oeps
/// toChar: return a string, formed from a array of numbers
/// if the array contains numbers that are not valid characters, it throws an exception
INTERNAL tlHandle _array_toChar(tlArgs* args) {
    tlArray* array = tlArrayAs(tlArgsTarget(args));

    int size = tlArraySize(array);
    char* buf = malloc_atomic(size + 1);
    assert(buf);
    for (int i = 0; i < size; i++) {
        int ch = tl_int_or(tlArrayGet(array, i), -1);
        if (ch < 0) ch = 32;
        if (ch > 255) ch = 32;
        if (ch < 0) { free(buf); TL_THROW("not a array of characters"); }
        if (ch > 255) { free(buf); TL_THROW("utf8 not yet supported"); }
        buf[i] = ch;
    }
    buf[size] = 0;
    return tlStringFromTake(buf, size);
}


static tlObject* arrayClass;
static void array_init() {
    _tlArrayKind.klass = tlClassObjectFrom(
        "toList", _array_toList,
        "size", _array_size,
        "clear", _array_clear,
        "get", _array_get,
        "set", _array_set,
        "add", _array_add,
        "cat", _array_cat,
        "pop", _array_pop,
        "remove", _array_remove,
        "insert", _array_insert,
        "slice", _array_slice,
        "splice", _array_splice,
        "toChar", _array_toChar,
        "addAll", null,
        "random", null,
        "each", null,
        "map", null,
        "find", null,
        "filter", null,
        "reduce", null,
        "min", null,
        "max", null,
        "sum", null,
        "mean", null,
        "median", null,
        "sort", null,
        "flatten", null,
        "join", null,
        null
    );
    arrayClass = tlClassObjectFrom(
        "new", _Array_new,
        "_methods", null,
        null
    );
    tlObjectSet_(arrayClass, s__methods, _tlArrayKind.klass);

    INIT_KIND(tlArrayKind);
}

static void array_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Array"), arrayClass);
}

