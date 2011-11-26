// object type, a fully mutable map accessed like an object

#include "trace-on.h"

static tlClass _tlObjectClass = { .name = "Object", };
tlClass* tlObjectClass = &_tlObjectClass;

struct tlObject {
    tlLock lock;
    // TODO upgrade to something ... ahem ... more appropriate
    tlMap* map;
};

tlObject* tlObjectNew(tlTask* task) {
    tlObject* obj = tlAlloc(task, tlObjectClass, sizeof(tlObject));
    obj->map = tlMapEmpty();
    return obj;
}
INTERNAL tlValue _Object_new(tlTask* task, tlArgs* args) {
    tlMap* map = (tlMap*)tlValueObjectCast(tlArgsGet(args, 0));
    if (!map) TL_THROW("expected a Value Object");
    tlObject* obj = tlObjectNew(task);
    obj->map = map;
    return obj;
}
INTERNAL tlValue _this_get(tlTask* task, tlArgs* args) {
    tlObject* obj = tlObjectCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Object");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    assert(obj->map);

    // TODO actually "call" the field
    return tlMapGetSym(obj->map, msg);
}
INTERNAL tlValue _this_set(tlTask* task, tlArgs* args) {
    tlObject* obj = tlObjectCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Object");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    tlValue val = tlArgsGet(args, 2);
    if (!val) val = tlNull;
    assert(obj->map);

    // TODO we should actually allow for setters and such ...
    obj->map = tlMapSet(task, obj->map, msg, val);
    return val;
}

static const tlNativeCbs __object_nativecbs[] = {
    { "_Object_new", _Object_new },
    { "_this_get", _this_get },
    { "_this_set", _this_set },
    { 0, 0 }
};

static void object_init() {
    _tlObjectClass.send = tlLockReceive;
    tl_register_natives(__object_nativecbs);
}

