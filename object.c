// object type, a fully mutable map accessed like an object

#include "trace-on.h"

static tlClass _tlObjectClass = { .name = "Object", .locked = true };
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
INTERNAL tlValue objectSend(tlTask* task, tlArgs* args) {
    tlObject* obj = tlObjectCast(tlArgsTarget(args));
    if (!obj) TL_THROW("expected an Object");
    assert(tlLockOwner(tlLockAs(obj)) == task);
    tlSym msg = tlArgsMsg(args);
    assert(msg);
    assert(obj->map);

    tlValue field = tlMapGetSym(obj->map, msg);
    if (!field) TL_THROW("%s.%s is undefined", tl_str(obj), tl_str(msg));
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(task, args, field);
}
INTERNAL tlValue _this_get(tlTask* task, tlArgs* args) {
    tlObject* obj = tlObjectCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Object");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    assert(obj->map);

    tlValue res = tlMapGetSym(obj->map, msg);
    return res?res:tlNull;
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
INTERNAL tlValue _this_send(tlTask* task, tlArgs* args) {
    tlObject* obj = tlObjectCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Object");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    trace("%s.%s(%d)", tl_str(obj), tl_str(msg), tlArgsSize(args) - 2);
    assert(obj->map);
    tlValue field = tlMapGetSym(obj->map, msg);
    if (!field) TL_THROW("%s.%s is undefined", tl_str(obj), tl_str(msg));
    if (!tlCallableIs(field)) return field;


    tlArgs* nargs = tlArgsNew(task, null, null);
    nargs->target = obj;
    nargs->msg = msg;
    nargs->list = tlListSlice(task, args->list, 2, tlListSize(args->list));
    nargs->map = args->map;

    return tlEvalArgsFn(task, nargs, field);
}

static const tlNativeCbs __object_nativecbs[] = {
    { "_Object_new", _Object_new },
    { "_this_send", _this_send },
    { "_this_get", _this_get },
    { "_this_set", _this_set },
    { 0, 0 }
};

static void object_init() {
    _tlObjectClass.send = objectSend;
    tl_register_natives(__object_nativecbs);
}

