#ifndef _queue_h_
#define _queue_h_

#include "tl.h"
#include "platform.h"

TL_REF_TYPE(tlMsgQueue);
TL_REF_TYPE(tlMsgQueueInput);
TL_REF_TYPE(tlMessage);

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

tlMsgQueue* tlMsgQueueNew();
bool tlMsgQueueIsEmpty(tlMsgQueue* queue);

tlMessage* tlMessageNew(tlTask* task, tlArgs* args);

tlHandle tlMessageReply(tlMessage* msg, tlHandle res);
tlHandle tlMessageThrow(tlMessage* msg, tlHandle res);
tlTask* tlMessageGetSender(tlMessage* msg);

void queue_init();
void queue_vm_default(tlVm* vm);

#endif
