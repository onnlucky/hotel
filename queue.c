// author: Onne Gorter, license: MIT (see license.txt)
// a native and language level message queue

TL_REF_TYPE(tlQueue);
static tlClass _tlQueueClass = { .name = "MsgQueue" };
tlClass* tlQueueClass = &_tlQueueClass;

TL_REF_TYPE(tlQueueInput);
static tlClass _tlQueueInputClass = { .name = "MsgQueueInput" };
tlClass* tlQueueInputClass = &_tlQueueInputClass;

TL_REF_TYPE(tlMessage);
static tlClass _tlMessageClass = { .name = "Message" };
tlClass* tlMessageClass = &_tlMessageClass;

struct tlQueue {
    tlHead head;
    lqueue msg_q;
    lqueue wait_q;
    tlQueueInput* input;
};
struct tlQueueInput {
    tlHead head;
    tlQueue* queue;
};
struct tlMessage {
    tlHead head;
    tlTask* sender;
    tlArgs* args;
};

tlMessage* tlMessageNew(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlAlloc(task, tlMessageClass, sizeof(tlMessage));
    msg->sender = task;
    msg->args = args;
    return msg;
}
tlQueue* tlQueueNew(tlTask* task) {
    tlQueue* queue = tlAlloc(task, tlQueueClass, sizeof(tlQueue));
    queue->input = tlAlloc(task, tlQueueInputClass, sizeof(tlQueueInput));
    queue->input->queue = queue;
    return queue;
}

INTERNAL void queueSignal(tlQueue* queue) {
    tlTask* task = tlTaskFromEntry(lqueue_get(&queue->wait_q));
    if (task) tlTaskReady(task);
}

INTERNAL tlValue resumeReply(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    trace("");
    return task->value;
}
INTERNAL tlValue resumeEnqueue(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlArgs* args = tlArgsAs(res);
    tlQueue* queue = tlQueueInputAs(tlArgsTarget(args))->queue;
    tlMessage* msg = tlMessageNew(task, args);
    trace("queue.send: %s %s", tl_str(queue), tl_str(msg));
    task->value = msg;
    task->frame = tlFrameSetResume(task, frame, resumeReply);

    tlTaskWaitFor(task, null);
    lqueue_put(&queue->msg_q, &task->entry);
    queueSignal(queue);
    return tlTaskNotRunning;
}
INTERNAL tlValue queueInputReceive(tlTask* task, tlArgs* args) {
    tlQueueInput* input = tlQueueInputCast(tlArgsTarget(args));
    if (!input) TL_THROW("expected a Queue");
    return tlTaskPauseResuming(task, resumeEnqueue, args);
}

INTERNAL tlValue resumeGet(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlArgs* args = tlArgsAs(res);
    tlQueue* queue = tlQueueCast(tlArgsTarget(args));
    trace("queue.get 2: %s", tl_str(queue));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;

    tlTaskWaitFor(task, null);
    lqueue_put(&queue->wait_q, &task->entry);
    return tlTaskNotRunning;
}
INTERNAL tlValue _queue_get(tlTask* task, tlArgs* args) {
    tlQueue* queue = tlQueueCast(tlArgsTarget(args));
    if (!queue) TL_THROW("expected a Queue");
    trace("queue.get: %s", tl_str(task));

    tlTask* sender = tlTaskFromEntry(lqueue_get(&queue->msg_q));
    trace("SENDER: %s", tl_str(sender));
    if (sender) return sender->value;
    return tlTaskPauseResuming(task, resumeGet, args);
}
INTERNAL tlValue _queue_input(tlTask* task, tlArgs* args) {
    tlQueue* queue = tlQueueCast(tlArgsTarget(args));
    if (!queue) TL_THROW("expected a Queue");
    return queue->input;
}
INTERNAL tlValue _Queue_new(tlTask* task, tlArgs* args) {
    return tlQueueNew(task);
}

INTERNAL tlValue _message_reply(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    trace("msg.reply: %s", tl_str(msg));
    // TODO do multiple return ...
    tlValue res = tlArgsGet(args, 0);
    if (!res) res = tlNull;
    msg->sender->value = res;
    tlTaskReady(msg->sender);
    return tlNull;
}
INTERNAL tlValue _message_name(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    return tlArgsMsg(msg->args);
}
INTERNAL tlValue _message_get(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsTarget(args));
    if (!msg) TL_THROW("expected a Message");
    tlValue key = tlArgsGet(args, 0);
    trace("msg.get: %s %s", tl_str(msg), tl_str(key));
    if (tlIntIs(key)) return tlArgsGet(msg->args, tl_int(key));
    return tlArgsMapGet(msg->args, key);
}

static tlMap* queueClass;
void queue_init() {
    _tlQueueInputClass.send = queueInputReceive;
    _tlQueueClass.map = tlClassMapFrom(
        "input", _queue_input,
        "get", _queue_get,
        null
    );
    _tlMessageClass.map = tlClassMapFrom(
        "reply", _message_reply,
        "name", _message_name,
        "get", _message_get,
        null
    );
    queueClass = tlClassMapFrom("new", _Queue_new, null);
}
static void queue_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("MsgQueue"), queueClass);
}

