// author: Onne Gorter, license: MIT (see license.txt)
// a native and language level message queue

#include "trace-off.h"

TL_REF_TYPE(tlQueue);
static tlKind _tlQueueKind = { .name = "Queue" };
tlKind* tlQueueKind;

TL_REF_TYPE(tlMsgQueue);
static tlKind _tlMsgQueueKind = { .name = "MsgQueue" };
tlKind* tlMsgQueueKind;

TL_REF_TYPE(tlMsgQueueInput);
static tlKind _tlMsgQueueInputKind = { .name = "MsgQueueInput" };
tlKind* tlMsgQueueInputKind;

TL_REF_TYPE(tlMessage);
static tlKind _tlMessageKind = { .name = "Message" };
tlKind* tlMessageKind;


typedef void(*tlMsgQueueSignalFn)(void);

struct tlQueue {
    tlHead head;
    // TODO make more efficient? And we can also swap stuff ...
    pthread_mutex_t lock;
    lqueue add_q;
    lqueue get_q;
};

struct tlMsgQueue {
    tlHead head;
    lqueue msg_q;
    lqueue wait_q;
    tlMsgQueueSignalFn signalcb;
    tlMsgQueueInput* input;
};
struct tlMsgQueueInput {
    tlHead head;
    tlMsgQueue* queue;
};
struct tlMessage {
    tlHead head;
    tlTask* sender;
    tlArgs* args;
};


tlQueue* tlQueueNew(int buffer) {
    tlQueue* queue = tlAlloc(tlQueueKind, sizeof(tlQueue));
    pthread_mutex_init(&queue->lock, null);
    assert(queue);
    return queue;
}
tlHandle _Queue_new(tlTask* task, tlArgs* args) {
    int size = tl_int_or(tlArgsGet(args, 0), 0);
    return tlQueueNew(size);
}

tlHandle _queue_add(tlTask* task, tlArgs* args) {
    tlQueue* queue = tlQueueAs(tlArgsTarget(args));

    pthread_mutex_lock(&queue->lock);
    if (!queue->add_q.head) {
        // we are first
        tlTask* getter = tlTaskFromEntry(lqueue_get(&queue->get_q));
        if (getter) {
            getter->value = tlResultFromArgs(args);
            trace("found an getter ... giving it values: %s", tl_str(getter->value));
            tlTaskReady(getter);
            pthread_mutex_unlock(&queue->lock);
            return tlNull;
        }
    }

    // enqueue as we are not first, or there is no getter
    trace("waiting for getter");
    task->value = args;
    tlTaskWaitFor(task, null);
    lqueue_put(&queue->add_q, &task->entry);
    pthread_mutex_unlock(&queue->lock);
    return null;
}

tlHandle _queue_get(tlTask* task, tlArgs* args) {
    tlQueue* queue = tlQueueAs(tlArgsTarget(args));

    pthread_mutex_lock(&queue->lock);
    if (!queue->get_q.head) {
        // we are first
        tlTask* adder = tlTaskFromEntry(lqueue_get(&queue->add_q));
        if (adder) {
            trace("found an adder ... taking its values %s", tl_str(adder->value));
            tlArgs* res = tlArgsAs(adder->value);
            adder->value = tlNull;
            tlTaskReady(adder);
            pthread_mutex_unlock(&queue->lock);
            return tlResultFromArgs(res);
        }
    }

    // enqueue as we are not first, or there is no adder
    trace("waiting for adder");
    tlTaskWaitFor(task, null);
    lqueue_put(&queue->get_q, &task->entry);
    pthread_mutex_unlock(&queue->lock);
    return null;
}

tlHandle _queue_poll(tlTask* task, tlArgs* args) {
    tlQueue* queue = tlQueueAs(tlArgsTarget(args));

    pthread_mutex_lock(&queue->lock);
    if (!queue->get_q.head) {
        // we are first
        tlTask* adder = tlTaskFromEntry(lqueue_get(&queue->add_q));
        if (adder) {
            trace("found an adder ... taking its values %s", tl_str(adder->value));
            tlArgs* res = tlArgsAs(adder->value);
            adder->value = tlNull;
            tlTaskReady(adder);
            pthread_mutex_unlock(&queue->lock);
            return tlResultFromArgs(res);
        }
    }
    pthread_mutex_unlock(&queue->lock);
    return tlNull;
}


// Message Queue

tlMessage* tlMessageNew(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlAlloc(tlMessageKind, sizeof(tlMessage));
    msg->sender = task;
    msg->args = args;
    return msg;
}
tlMsgQueue* tlMsgQueueNew() {
    tlMsgQueue* queue = tlAlloc(tlMsgQueueKind, sizeof(tlMsgQueue));
    queue->input = tlAlloc(tlMsgQueueInputKind, sizeof(tlMsgQueueInput));
    queue->input->queue = queue;
    return queue;
}
bool tlMsgQueueIsEmpty(tlMsgQueue* queue) {
    return lqueue_peek(&queue->msg_q) == null;
}

INTERNAL void queueSignal(tlMsgQueue* queue) {
    if (queue->signalcb) queue->signalcb();
    tlTask* task = tlTaskFromEntry(lqueue_get(&queue->wait_q));
    if (!task) return;

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (!sender) return;
    task->value = sender->value;
    tlTaskReady(task);
}

tlHandle tlMessageReply(tlMessage* msg, tlHandle res) {
    trace("msg.reply: %s", tl_str(msg));
    if (!res) res = tlNull;
    msg->sender->value = res;
    tlTaskReady(msg->sender);
    return res;
}
tlHandle tlMessageThrow(tlMessage* msg, tlHandle res) {
    trace("msg.throw: %s", tl_str(msg));
    if (!res) res = tlNull;
    msg->sender->value = res;
    msg->sender->hasError = true;
    tlTaskReady(msg->sender);
    return res;
}
tlTask* tlMessageGetSender(tlMessage* msg) {
    return msg->sender;
}

INTERNAL tlHandle queueInputReceive(tlTask* task, tlArgs* args, bool safe) {
    tlMsgQueue* queue = tlMsgQueueInputAs(tlArgsTarget(args))->queue;
    tlMessage* msg = tlMessageNew(task, args);
    trace("queue.send: %s %s", tl_str(queue), tl_str(msg));
    task->value = msg;

    tlTaskWaitNothing1(task);
    lqueue_put(&queue->msg_q, &task->entry);
    queueSignal(queue);
    tlTaskWaitNothing2(task);
    return null;
}

INTERNAL tlHandle _msg_queue_get(tlTask* task, tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueAs(tlArgsTarget(args));
    trace("queue.get: %s", tl_str(tlTaskCurrent()));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;

    tlTaskWaitNothing1(task);
    lqueue_put(&queue->wait_q, &task->entry);
    tlTaskWaitNothing2(task);
    return null;
}

INTERNAL tlHandle _msg_queue_poll(tlTask* task, tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueAs(tlArgsTarget(args));
    trace("queue.get: %s", tl_str(tlTaskCurrent()));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;
    return tlNull;
}

INTERNAL tlHandle _msg_queue_input(tlTask* task, tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueAs(tlArgsTarget(args));
    return queue->input;
}
INTERNAL tlHandle _MsgQueue_new(tlTask* task, tlArgs* args) {
    return tlMsgQueueNew();
}

INTERNAL tlHandle _message_reply(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageAs(tlArgsTarget(args));
    // TODO do multiple return ...
    return tlMessageReply(msg, tlArgsGet(args, 0));
}
INTERNAL tlHandle _message_throw(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageAs(tlArgsTarget(args));
    // TODO do multiple return ...
    return tlMessageThrow(msg, tlArgsGet(args, 0));
}
INTERNAL tlHandle _message_name(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageAs(tlArgsTarget(args));
    return tlArgsMethod(msg->args);
}
INTERNAL tlHandle _message_get(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageAs(tlArgsTarget(args));
    tlHandle key = tlArgsGet(args, 0);
    trace("msg.get: %s %s", tl_str(msg), tl_str(key));
    if (tlIntIs(key)) return tlMAYBE(tlArgsGet(msg->args, at_offset(key, tlArgsSize(msg->args))));
    return tlArgsGetNamed(msg->args, key);
}

static tlObject* queueClass;
static tlObject* msgQueueClass;

void queue_init() {
    queueClass = tlClassObjectFrom("new", _Queue_new, null);
    _tlQueueKind.klass = tlClassObjectFrom(
        "add", _queue_add,
        "get", _queue_get,
        "poll", _queue_poll,
        null
    );

    msgQueueClass = tlClassObjectFrom("new", _MsgQueue_new, null);
    _tlMsgQueueInputKind.send = queueInputReceive;
    _tlMsgQueueKind.klass = tlClassObjectFrom(
        "input", _msg_queue_input,
        "get", _msg_queue_get,
        "poll", _msg_queue_poll,
        null
    );
    _tlMessageKind.klass = tlClassObjectFrom(
        "reply", _message_reply,
        "throw", _message_throw,
        "name", _message_name,
        "get", _message_get,
        null
    );

    INIT_KIND(tlQueueKind);
    INIT_KIND(tlMsgQueueKind);
    INIT_KIND(tlMsgQueueInputKind);
    INIT_KIND(tlMessageKind);
}
static void queue_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Queue"), queueClass);
   tlVmGlobalSet(vm, tlSYM("MsgQueue"), msgQueueClass);
}

