// object type, a fully mutable map accessed like an object

#include "trace-off.h"

static tlKind _tlObjectKind = { .name = "Object", .locked = true };
tlKind* tlObjectKind = &_tlObjectKind;

struct tlObject {
    tlLock lock;
    // TODO upgrade to something ... ahem ... more appropriate
    tlMap* map;
};

tlObject* tlObjectNew() {
    tlObject* obj = tlAlloc(tlObjectKind, sizeof(tlObject));
    obj->map = tlMapEmpty();
    return obj;
}
INTERNAL tlValue _Object_new(tlArgs* args) {
    tlMap* map = (tlMap*)tlValueObjectCast(tlArgsGet(args, 0));
    if (!map) map = tlMapEmpty();
    tlObject* obj = tlObjectNew();
    obj->map = map;
    return obj;
}
INTERNAL tlValue objectSend(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlObject* obj = tlObjectCast(tlArgsTarget(args));
    if (!obj) TL_THROW("expected an Object");
    assert(tlLockOwner(tlLockAs(obj)) == task);
    tlSym msg = tlArgsMsg(args);
    assert(msg);
    assert(obj->map);

    tlValue field = tlMapGetSym(obj->map, msg);
    if (!field) TL_THROW("%s.%s is undefined", tl_str(obj), tl_str(msg));
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(args, field);
}
INTERNAL tlValue _this_get(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlObject* obj = tlObjectCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Object");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    assert(obj->map);

    tlValue res = tlMapGetSym(obj->map, msg);
    return res?res:tlNull;
}
INTERNAL tlValue _this_set(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlObject* obj = tlObjectCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Object");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    tlValue val = tlArgsGet(args, 2);
    if (!val) val = tlNull;
    assert(obj->map);

    // TODO we should actually allow for setters and such ...
    obj->map = tlMapSet(obj->map, msg, val);
    return val;
}
INTERNAL tlValue _this_send(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
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


    tlArgs* nargs = tlArgsNew(null, null);
    nargs->target = obj;
    nargs->msg = msg;
    nargs->list = tlListSlice(args->list, 2, tlListSize(args->list));
    nargs->map = args->map;

    return tlEvalArgsFn(nargs, field);
}

static const tlNativeCbs __object_nativecbs[] = {
    { "_Object_new", _Object_new },
    { "_this_send", _this_send },
    { "_this_get", _this_get },
    { "_this_set", _this_set },
    { 0, 0 }
};

static void object_init() {
    _tlObjectKind.send = objectSend;
    tl_register_natives(__object_nativecbs);
}

