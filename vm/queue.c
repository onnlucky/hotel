// author: Onne Gorter, license: MIT (see license.txt)
// a native and language level message queue

#include "trace-off.h"

TL_REF_TYPE(tlQueue);
static tlKind _tlQueueKind = { .name = "Queue" };
tlKind* tlQueueKind = &_tlQueueKind;

TL_REF_TYPE(tlTaskQueue);
static tlKind _tlTaskQueueKind = { .name = "TaskQueue" };
tlKind* tlTaskQueueKind = &_tlTaskQueueKind;

TL_REF_TYPE(tlMsgQueue);
static tlKind _tlMsgQueueKind = { .name = "MsgQueue" };
tlKind* tlMsgQueueKind = &_tlMsgQueueKind;

TL_REF_TYPE(tlMsgQueueInput);
static tlKind _tlMsgQueueInputKind = { .name = "MsgQueueInput" };
tlKind* tlMsgQueueInputKind = &_tlMsgQueueInputKind;

TL_REF_TYPE(tlMessage);
static tlKind _tlMessageKind = { .name = "Message" };
tlKind* tlMessageKind = &_tlMessageKind;

typedef void(*tlMsgQueueSignalFn)(void);

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

tlMessage* tlMessageNew(tlArgs* args) {
    tlMessage* msg = tlAlloc(tlMessageKind, sizeof(tlMessage));
    msg->sender = tlTaskCurrent();
    msg->args = args;
    return msg;
}
tlMsgQueue* tlMsgQueueNew() {
    tlMsgQueue* queue = tlAlloc(tlMsgQueueKind, sizeof(tlMsgQueue));
    queue->input = tlAlloc(tlMsgQueueInputKind, sizeof(tlMsgQueueInput));
    queue->input->queue = queue;
    return queue;
}

INTERNAL void queueSignal(tlMsgQueue* queue) {
    if (queue->signalcb) queue->signalcb();
    tlTask* task = tlTaskFromEntry(lqueue_get(&queue->wait_q));
    if (task) tlTaskReady(task);
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
    msg->sender->value = null;
    msg->sender->throw = res;
    tlTaskReady(msg->sender);
    return res;
}
tlTask* tlMessageGetSender(tlMessage* msg) {
    return msg->sender;
}

INTERNAL tlHandle resumeReply(tlFrame* frame, tlHandle res, tlHandle throw) {
    trace("");
    return tlTaskCurrent()->value;
}
INTERNAL tlHandle resumeEnqueue(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlTask* task = tlTaskCurrent();
    tlArgs* args = tlArgsAs(res);
    tlMsgQueue* queue = tlMsgQueueInputAs(tlArgsTarget(args))->queue;
    tlMessage* msg = tlMessageNew(args);
    trace("queue.send: %s %s", tl_str(queue), tl_str(msg));
    task->value = msg;
    task->stack = tlFrameSetResume(frame, resumeReply);

    tlTaskWaitFor(null);
    lqueue_put(&queue->msg_q, &task->entry);
    queueSignal(queue);
    return tlTaskNotRunning;
}
INTERNAL tlHandle queueInputReceive(tlArgs* args) {
    tlMsgQueueInput* input = tlMsgQueueInputCast(tlArgsTarget(args));
    if (!input) TL_THROW("expected a Queue");
    return tlTaskPauseResuming(resumeEnqueue, args);
}

INTERNAL tlHandle resumeGet(tlFrame* frame, tlHandle res, tlHandle throw) {
    tlArgs* args = tlArgsAs(res);
    tlMsgQueue* queue = tlMsgQueueCast(tlArgsTarget(args));
    trace("queue.get 2: %s", tl_str(queue));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;

    tlTask* task = tlTaskCurrent();
    tlTaskWaitFor(null);
    lqueue_put(&queue->wait_q, &task->entry);
    return tlTaskNotRunning;
}
INTERNAL tlHandle _queue_get(tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueCast(tlArgsTarget(args));
    if (!queue) TL_THROW("expected a Queue");
    trace("queue.get: %s", tl_str(task));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;
    return tlTaskPauseResuming(resumeGet, args);
}
INTERNAL tlHandle _queue_poll(tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueCast(tlArgsTarget(args));
    if (!queue) TL_THROW("expected a Queue");
    trace("queue.get: %s", tl_str(task));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;
    return tlNull;
}
INTERNAL tlHandle _queue_input(tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueCast(tlArgsTarget(args));
    if (!queue) TL_THROW("expected a Queue");
    return queue->input;
}
INTERNAL tlHandle _Queue_new(tlArgs* args) {
    return tlMsgQueueNew();
}

INTERNAL tlHandle _message_reply(tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    // TODO do multiple return ...
    return tlMessageReply(msg, tlArgsGet(args, 0));
}
INTERNAL tlHandle _message_throw(tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    // TODO do multiple return ...
    return tlMessageThrow(msg, tlArgsGet(args, 0));
}
INTERNAL tlHandle _message_name(tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    return tlArgsMsg(msg->args);
}
INTERNAL tlHandle _message_get(tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    tlHandle key = tlArgsGet(args, 0);
    trace("msg.get: %s %s", tl_str(msg), tl_str(key));
    if (tlIntIs(key)) return tlArgsGet(msg->args, at_offset(key, tlArgsSize(msg->args)));
    return tlArgsMapGet(msg->args, key);
}

static tlMap* queueClass;
void queue_init() {
    _tlMsgQueueInputKind.send = queueInputReceive;
    _tlMsgQueueKind.klass = tlClassMapFrom(
        "input", _queue_input,
        "get", _queue_get,
        "poll", _queue_poll,
        null
    );
    _tlMessageKind.klass = tlClassMapFrom(
        "reply", _message_reply,
        "throw", _message_throw,
        "name", _message_name,
        "get", _message_get,
        null
    );
    queueClass = tlClassMapFrom("new", _Queue_new, null);
}
static void queue_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("MsgQueue"), queueClass);
}

