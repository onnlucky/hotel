// object type, a hotel object is a thread/actore/mutable object all in one

#include "trace-off.h"

static tlClass tlObjectClass;

struct tlObject {
    tlHead head;
    lqueue msg_q;
    tlTask* owner;
    tlMap* map;
};
typedef struct tlPauseSend {
    tlPause pause;
    tlObject* sender;
    tlArgs* args;
} tlPauseSend;

void* tlPauseAlloc(tlTask* task, size_t bytes, int fields, tlResumeCb resume) {
    return null;
}
tlPause* tlTaskPause(tlTask* task, void*);
tlPause* tlTaskAttachPause(tlTask* task, void*);

tlObject* tlObjectNew(tlTask* task) {
    tlObject* self = TL_ALLOC(Object, 0);
    self->map = tlmap_empty();
    self->head.klass = &tlObjectClass;
    return self;
}
tlPause* _new_object(tlTask* task, tlArgs* args) {
    TL_RETURN(tlObjectNew(task));
}

tlPause* _ResumeSend(tlTask* task, tlPause* run);
tlPause* _ObjectSend(tlTask* task, tlArgs* args) {
    tlObject* sender = task->object;
    tlObject* self = tlobject_cast(args->target);
    tlSym* msg = tlsym_cast(args->msg);
    assert(self);
    assert(msg);

    if (self->owner) {
        fatal("not implemented");
        /*
        tlPauseSend* pause = tlPauseAlloc(task, sizeof(tlPauseSend), 0, _ResumeSend);
        pause->sender = sender;
        pause->args = args;
        // TODO put in msg queue after tlTaskPause is done ... but that is impossible now
        lqueue_put(&self->msg_q, &task->entry);
        return tlTaskPause(task, pause);
        */
    } else {
        self->owner = task;

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
        task->object = self;
        args->fn = field;
        fatal("not implemented");
        /*
        tlPause* paused = null; //start_arg(task, args);
        if (paused) {
            tlPauseSend* pause = tlPauseAlloc(task, sizeof(tlPauseSend), 0, _ResumeSend);
            return tlTaskAttachPause(task, pause);
        }
        */
        task->object = sender;
        self->owner = null;
    }
    return null;
}

static tlClass tlObjectClass = {
    .name = "object",
    .map = null,
    .send = _ObjectSend
};

