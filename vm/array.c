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
        assert(array->alloc && array->alloc >= array->size + 8);
        assert(array->data);
    }
    array->data[array->size++] = v;
    return array;
}
INTERNAL tlArray* tlArrayPop(tlArray* array) {
    if (array->size == 0) return tlUndefined;
    assert(array->data);
    array->size--;
    tlValue v = array->data[array->size];
    array->data[array->size] = 0;
    return v;
}

INTERNAL tlValue _Array_new(tlTask* task, tlArgs* args) {
    return tlArrayNew(task);
}
INTERNAL tlValue _array_size(tlTask* task, tlArgs* args) {
    tlArray* array = tlArrayCast(tlArgsTarget(args));
    if (!array) TL_THROW("expected an Array");
    return tlINT(array->size);
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
    return tlArrayPop(array);
}

static tlMap* arrayClass;
static void array_init() {
    _tlArrayClass.map = tlClassMapFrom(
        "size", _array_size,
        "add", _array_add,
        "pop", _array_pop,
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

