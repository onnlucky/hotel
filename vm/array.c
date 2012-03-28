// a mutable list implementation

#include "trace-on.h"

static tlClass _tlArrayClass = { .name = "List", .locked = true };
tlClass* tlArrayClass = &_tlArrayClass;

TL_REF_TYPE(tlArray);

struct tlArray {
    tlLock lock;
    intptr_t size;
    intptr_t alloc;
    tlValue* data;
};

INTERNAL tlArray* tlArrayNew(tlTask* task) {
    tlArray* array = tlAlloc(task, tlArrayClass, sizeof(tlArray));
    return array;
}
INTERNAL tlArray* tlArrayAdd(tlTask* task, tlArray* array, tlValue v) {
    trace("add: %s.add %s -- %zd -- %zd", tl_str(array), tl_str(v), array->size, array->alloc);
    if (array->size == array->alloc) {
        if (!array->alloc) array->alloc = 8; else array->alloc *= 2;
        array->data = realloc(array->data, array->alloc * sizeof(tlValue));
        assert(array->data);
    }
    array->data[array->size++] = v;
    return array;
}
INTERNAL tlValue tlArrayPop(tlTask* task, tlArray* array) {
    if (array->size == 0) return tlUndefined;
    array->size--;
    tlValue v = array->data[array->size];
    array->data[array->size] = 0;
    return v;
}
INTERNAL tlValue tlArrayRemove(tlTask* task, tlArray* array, int i) {
    if (i < 0) return tlUndefined;
    if (i >= array->size) return tlUndefined;
    tlValue v = array->data[i];

    trace("remove: %d", i);
    memmove(array->data + i, array->data + i + 1, (array->size - i) * sizeof(tlValue));
    array->size -= 1;

    return v;
}
INTERNAL tlArray* tlArrayInsert(tlTask* task, tlArray* array, int i, tlValue v) {
    if (i < 0) return array;
    if (i == array->size) return tlArrayAdd(task, array, v);

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
        array->data = realloc(array->data, array->alloc * sizeof(tlValue));
        assert(array->data);
    }

    if (nsize - array->size > 0) {
        trace("clearing: %zd - %zd", array->size, nsize - array->size);
        memset(array->data + array->size, 0, (nsize - array->size) * sizeof(tlValue));
    }
    if (array->size - i > 0) {
        trace("moving: %d - %zd", i + 1, array->size - i);
        memmove(array->data + i + 1, array->data + i, (array->size - i) * sizeof(tlValue));
    }
    array->data[i] = v;
    array->size = nsize;

    return array;
}
INTERNAL tlValue tlArrayGet(tlArray* array, int i) {
    if (i < 0) return tlUndefined;
    if (i >= array->size) return tlUndefined;
    return array->data[i];
}
INTERNAL tlArray* tlArraySet(tlTask* task, tlArray* array, int i, tlValue v) {
    if (i < 0) return tlUndefined;
    if (i == array->size) return tlArrayAdd(task, array, v);
    if (i > array->size) return tlArrayInsert(task, array, i, v);
    array->data[i] = v;
    return array;
}

INTERNAL tlValue _Array_new(tlTask* task, tlArgs* args) {
    return tlArrayNew(task);
}
INTERNAL tlValue _array_size(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    return tlINT(array->size);
}
INTERNAL tlValue _array_get(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    tlValue v = tlArrayGet(array, tl_int(i));
    return v?v:tlNull;
}
INTERNAL tlValue _array_set(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    return tlArraySet(task, array, tl_int(i), tlArgsGet(args, 1));
}
INTERNAL tlValue _array_add(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlValue v = tlArgsGet(args, i);
        tlArrayAdd(task, array, v);
    }
    return array;
}
INTERNAL tlValue _array_pop(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlValue v = tlArrayPop(task, array);
    return v?v:tlNull;
}
INTERNAL tlValue _array_insert(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    return tlArrayInsert(task, array, tl_int(i), tlArgsGet(args, 1));
}
INTERNAL tlValue _array_remove(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    tlInt i = tlIntCast(tlArgsGet(args, 0));
    if (!i) TL_THROW("expected a index");
    tlValue v = tlArrayRemove(task, array, tl_int(i));
    return v?v:tlNull;
}

static tlMap* arrayClass;
static void array_init() {
    _tlArrayClass.map = tlClassMapFrom(
        "size", _array_size,
        "get", _array_get,
        "set", _array_set,
        "add", _array_add,
        "pop", _array_pop,
        //"addFront", _array_addFront,
        //"popFront", _array_popFront,
        "remove", _array_remove,
        "insert", _array_insert,
        null
    );
    arrayClass = tlClassMapFrom(
        "new", _Array_new,
        null
    );
}

static void array_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Array"), arrayClass);
}

