// object type, a hotel object is a thread/actore/mutable object all in one

#include "trace-off.h"

static tlClass tlObjectClass;

struct tlObject {
    tlHead head;
    tlTask* owner;
    tlMap* map;
};

tlObject* tlObjectNew(tlTask* task) {
    tlObject* oop = TL_ALLOC(Object, 0);
    oop->map = tlmap_empty();
    oop->head.klass = &tlObjectClass;
    return oop;
}
tlRun* _new_object(tlTask* task, tlArgs* args) {
    TL_RETURN(tlObjectNew(task));
}

tlRun* _ObjectSend(tlTask* task, tlArgs* args) {
    tlObject* oop = tlobject_cast(args->target);
    tlSym* msg = tlsym_cast(args->msg);
    assert(oop);
    assert(msg);

    if (oop->owner) {
        fatal("already have a owner, implement");
    } else {
        tlValue field = tlmap_get(task, oop->map, msg);
        print("OBJECT SEND %p: %s -> %s", oop, tl_str(msg), tl_str(field));
        if (!field) TL_RETURN(tlUndefined);
        if (!tlcallable_is(field)) TL_RETURN(field);
        fatal("don't know how to run code");
        print("%s", tl_str(field));
        args->fn = field;
        //return start_args(task, args, null);
    }
    abort();
}

static tlClass tlObjectClass = {
    .name = "object",
    .map = null,
    .send = _ObjectSend
};

