// a mutable list implementation

#include "trace-off.h"

static tlKind _tlArrayKind = { .name = "Array", .locked = true };
tlKind* tlArrayKind = &_tlArrayKind;

TL_REF_TYPE(tlArray);

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
    if (array->size == 0) return tlUndefined;
    array->size--;
    tlHandle v = array->data[array->size];
    array->data[array->size] = 0;
    return v;
}
tlHandle tlArrayRemove(tlArray* array, int i) {
    if (i < 0) return tlUndefined;
    if (i >= array->size) return tlUndefined;
    tlHandle v = array->data[i];

    trace("remove: %d", i);
    memmove(array->data + i, array->data + i + 1, (array->size - i) * sizeof(tlHandle));
    array->size -= 1;

    return v;
}
tlArray* tlArrayInsert(tlArray* array, int i, tlHandle v) {
    if (i < 0) return array;
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
    if (i < 0) return tlUndefined;
    if (i >= array->size) return tlUndefined;
    return array->data[i];
}
tlArray* tlArraySet(tlArray* array, int i, tlHandle v) {
    if (i < 0) return tlUndefined;
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
    return tlArrayNew();
}
INTERNAL tlHandle _array_size(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    return tlINT(array->size);
}
INTERNAL tlHandle _array_toList(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    return tlArrayToList(array);
}
INTERNAL tlHandle _array_get(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    tlHandle v = tlArrayGet(array, tl_int(i));
    return v?v:tlNull;
}
INTERNAL tlHandle _array_set(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    return tlArraySet(array, tl_int(i), tlArgsGet(args, 1));
}
INTERNAL tlHandle _array_add(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlHandle v = tlArgsGet(args, i);
        tlArrayAdd(array, v);
    }
    return array;
}
INTERNAL tlHandle _array_pop(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlHandle v = tlArrayPop(array);
    return v?v:tlNull;
}
INTERNAL tlHandle _array_insert(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    return tlArrayInsert(array, tl_int(i), tlArgsGet(args, 1));
}
INTERNAL tlHandle _array_remove(tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    tlHandle v = tlArrayRemove(array, tl_int(i));
    return v?v:tlNull;
}

static tlMap* arrayClass;
static void array_init() {
    _tlArrayKind.klass = tlClassMapFrom(
        "toList", _array_toList,
        "size", _array_size,
        "get", _array_get,
        "set", _array_set,
        "add", _array_add,
        "pop", _array_pop,
        //"addFront", _array_addFront,
        //"popFront", _array_popFront,
        "remove", _array_remove,
        "insert", _array_insert,
        "random", null,
        "each", null,
        "join", null,
        null
    );
    arrayClass = tlClassMapFrom(
        "new", _Array_new,
        "class", null,
        null
    );
    tlMapSetSym_(arrayClass, s_class, _tlArrayKind.klass);
}

static void array_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Array"), arrayClass);
}

