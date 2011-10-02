// object type, a hotel object is a thread/actore/mutable object all in one

#include "trace-on.h"

static tlClass tlObjectClass;

struct tlObject {
    tlHead head;
    lqueue msg_q;
    tlTask* owner;
    tlMap* map;
};

typedef struct tlPauseBefore {
    tlPause pause;
    tlArgs* args;
} tlPauseBefore;

typedef struct tlPauseAfter {
    tlPause pause;
    tlObject* sender;
} tlPauseAfter;

tlObject* tlObjectNew(tlTask* task) {
    tlObject* self = TL_ALLOC(Object, 0);
    self->map = tlmap_empty();
    self->head.klass = &tlObjectClass;
    return self;
}
tlPause* _new_object(tlTask* task, tlArgs* args) {
    tlMap* map = tlmap_cast(tlargs_get(args, 0));
    tlObject* oop = tlObjectNew(task);
    oop->map = map;
    TL_RETURN(oop);
}

tlPause* _ObjectSend2(tlTask* task, tlArgs* args);

tlPause* _ResumeBefore(tlTask* task, tlPause* _pause) {
    trace("");
    tlPauseBefore* pause = (tlPauseBefore*)_pause;
    return _ObjectSend2(task, pause->args);
}
tlPause* _ResumeAfter(tlTask* task, tlPause* _pause) {
    trace("");
    tlPauseAfter* pause = (tlPauseAfter*)_pause;
    task->sender = pause->sender;
    task->pause = _pause->caller;
    return null;
}

tlPause* _ObjectSend(tlTask* task, tlArgs* args) {
    tlObject* self = tlobject_cast(args->target);
    assert(self);

    if (self->owner) {
        tlPauseBefore* pause = tlPauseAlloc(task, sizeof(tlPauseBefore), 0, _ResumeBefore);
        pause->args = args;
        // TODO put in msg queue after tlTaskPause is done ... but that is impossible now
        // TODO after putting ourself in the queue, we must try run the object
        lqueue_put(&self->msg_q, &task->entry);
        return tlTaskPause(task, pause);
    } else {
        return _ObjectSend2(task, args);
    }
}
tlPause* _ObjectSend2(tlTask* task, tlArgs* args) {
    tlObject* sender = task->sender;
    tlObject* self = tlobject_cast(args->target);
    tlSym msg = tlsym_cast(args->msg);
    assert(self);
    assert(msg);

    tlValue field = tlmap_get(task, self->map, msg);
    print("OBJECT SEND %p: %s -> %s", self, tl_str(msg), tl_str(field));
    if (!field) {
        self->owner = null;
        TL_RETURN(tlUndefined);
    }
    if (!tlcallable_is(field)) {
        self->owner = null;
        TL_RETURN(field);
    }
    task->sender = self;
    args->fn = field;
    tlPause* p = tlTaskEvalArgs(task, args);
    if (p) {
        tlPauseAfter* pause = tlPauseAlloc(task, sizeof(tlPauseAfter), 0, _ResumeAfter);
        pause->sender = sender;
        return tlTaskPauseAttach(task, p, pause);
    }
    task->sender = sender;
    self->owner = null;
    return null;
}

static tlClass tlObjectClass = {
    .name = "object",
    .map = null,
    .send = _ObjectSend
};

