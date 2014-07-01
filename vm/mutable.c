// mutable type, a fully mutable map accessed like an mutable

#include "trace-off.h"

static tlKind _tlMutableKind = { .name = "Mutable", .locked = true };
tlKind* tlMutableKind = &_tlMutableKind;

struct tlMutable {
    tlLock lock;
    // TODO upgrade to something ... ahem ... more appropriate
    tlObject* map;
};

tlMutable* tlMutableNew() {
    tlMutable* obj = tlAlloc(tlMutableKind, sizeof(tlMutable));
    obj->map = tlObjectEmpty();
    return obj;
}
INTERNAL tlHandle _Mutable_new(tlArgs* args) {
    tlObject* map = (tlObject*)tlObjectCast(tlArgsGet(args, 0));
    if (!map) map = tlObjectEmpty();
    tlMutable* obj = tlMutableNew();
    obj->map = map;
    return obj;
}
INTERNAL tlHandle mutableSend(tlArgs* args) {
    tlMutable* obj = tlMutableAs(tlArgsTarget(args));
    tlTask* task = tlTaskCurrent();
    assert(tlLockOwner(tlLockAs(obj)) == task);

    assert(tlArgsMsg(args));
    assert(obj->map);

    tlObject* map = obj->map;
    tlSym msg = tlArgsMsg(args);
    tlHandle field = null;
    tlObject* cls;

    // search for field
    cls = map;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(msg));
        field = tlObjectGet(cls, msg);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

    // search for getter
    if (msg == s__set) goto send;
    cls = map;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(s__get));
        field = tlObjectGet(cls, s__get);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

send:;
    trace("OBJECT SEND %p: %s -> %s", map, tl_str(msg), tl_str(field));

    if (!field) {
        if (msg == s__set) {
            tlHandle key = tlSymCast(tlArgsGet(args, 0));
            if (!key) return tlUndef();
            tlHandle val = tlArgsGet(args, 1);
            if (!val) val = tlNull;
            obj->map = tlObjectSet(obj->map, key, val);
            return val;
        }
        TL_THROW("%s.%s is undefined", tl_str(map), tl_str(msg));
    }
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(args, field);
}

INTERNAL tlHandle _this_get(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlMutable* obj = tlMutableCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Mutable");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    assert(obj->map);

    tlHandle res = tlObjectGetSym(obj->map, msg);
    return res?res:tlNull;
}
INTERNAL tlHandle _this_set(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlMutable* obj = tlMutableCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Mutable");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    tlHandle val = tlArgsGet(args, 2);
    if (!val) val = tlNull;
    assert(obj->map);

    // TODO we should actually allow for setters and such ...
    obj->map = tlObjectSet(obj->map, msg, val);
    return val;
}
INTERNAL tlHandle _this_send(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlMutable* obj = tlMutableCast(tlArgsGet(args, 0));
    if (!obj) TL_THROW("expected an Mutable");
    if (tlLockOwner(tlLockAs(obj)) != task) TL_THROW("task not owner of: %s", tl_str(obj));
    tlSym msg = tlSymCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expected a field");
    trace("%s.%s(%d)", tl_str(obj), tl_str(msg), tlArgsSize(args) - 2);
    assert(obj->map);
    tlHandle field = tlObjectGetSym(obj->map, msg);
    if (!field) TL_THROW("%s.%s is undefined", tl_str(obj), tl_str(msg));
    if (!tlCallableIs(field)) return field;


    tlArgs* nargs = tlArgsNew(null, null);
    nargs->target = obj;
    nargs->msg = msg;
    nargs->list = tlListSub(args->list, 2, tlListSize(args->list) - 2);
    nargs->map = args->map;

    return tlEvalArgsFn(nargs, field);
}

static const tlNativeCbs __mutable_nativecbs[] = {
    { "_Mutable_new", _Mutable_new },
    { "_this_send", _this_send },
    { "_this_get", _this_get },
    { "_this_set", _this_set },
    { 0, 0 }
};

static void mutable_init() {
    _tlMutableKind.send = mutableSend;
    tl_register_natives(__mutable_nativecbs);
}

