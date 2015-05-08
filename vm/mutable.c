// author: Onne Gorter, license: MIT (see license.txt)

// mutable type, a fully mutable map accessed like an mutable

#include "platform.h"
#include "mutable.h"

#include "value.h"
#include "set.h"
#include "object.h"
#include "args.h"

static tlKind _tlMutableKind = { .name = "Mutable", .locked = true };
tlKind* tlMutableKind;

struct tlMutable {
    tlLock lock;
    // TODO upgrade to something ... ahem ... more appropriate
    tlObject* data;
};

tlMutable* tlMutableNew(tlObject* from) {
    tlMutable* mut = tlAlloc(tlMutableKind, sizeof(tlMutable));
    mut->data = tlClone(from);
    return mut;
}
static tlHandle _Mutable_new(tlTask* task, tlArgs* args) {
    tlObject* map = tlObjectCast(tlArgsGet(args, 0));
    if (!map) map = tlObjectEmpty();
    return tlMutableNew(map);
}
/*
static tlHandle _Mutable_new_shareLock(tlTask* task, tlArgs* args) {
    tlHandle* share = tlArgsGet(args, 0);
    assert(tlLockIs(share));

    tlObject* map = (tlObject*)tlObjectCast(tlArgsGet(args, 1));
    if (!map) map = tlObjectEmpty();
    tlMutable* obj = tlMutableNew();
    fatal("cannot do this yet");
    //obj->lock = tlLockAs(share);
    obj->map = map;
    return obj;
}
*/
static tlHandle mutableSend(tlTask* task, tlArgs* args, bool safe) {
    tlMutable* mut = tlMutableAs(tlArgsTarget(args));
    assert(tlLockOwner(tlLockAs(mut)) == task);

    assert(tlArgsMethod(args));
    assert(mut->data);

    tlSym msg = tlArgsMethod(args);
    tlHandle field = null;
    tlObject* cls;

    if (tlArgsIsSetter(args)) print("SETTER: %s.%s = %s", tl_str(mut), tl_str(msg), tl_str(tlArgsGet(args, 0)));

    // search for field
    cls = mut->data;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(msg));
        field = tlObjectGet(cls, msg);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

    // search for getter
    if (msg == s__set) goto send;
    cls = mut->data;
    while (cls) {
        trace("CLASS: %s %s", tl_str(cls), tl_str(s__get));
        field = tlObjectGet(cls, s__get);
        if (field) goto send;
        cls = tlObjectGet(cls, s_class);
    }

send:;
    trace("OBJECT SEND %p: %s -> %s", mut->data, tl_str(msg), tl_str(field));

    if (!field) {
        if (msg == s__set) {
            tlHandle key = tlSymCast(tlArgsGet(args, 0));
            if (!key) return tlUndef();
            tlHandle val = tlArgsGet(args, 1);
            if (!val) val = tlNull;
            int at = tlSetIndexof(mut->data->keys, key);
            if (at >= 0) {
                mut->data->data[at] = val;
            } else {
                mut->data = tlObjectSet(mut->data, key, val);
            }
            return val;
        }
        if (safe) return tlNull;
        TL_THROW("%s.%s is undefined", tl_str(mut), tl_str(msg));
    }
    if (!tlCallableIs(field)) return field;
    return tlEvalArgsFn(task, args, field);
}

static const tlNativeCbs __mutable_nativecbs[] = {
    { "_Mutable_new", _Mutable_new },
    //{ "_Mutable_new_shareLock", _Mutable_new_shareLock },
    { 0, 0 }
};

void mutable_init() {
    _tlMutableKind.send = mutableSend;
    tl_register_natives(__mutable_nativecbs);

    INIT_KIND(tlMutableKind);
}

