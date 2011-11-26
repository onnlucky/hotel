// evaluation can pause at any time, this is how we capture those pauses

#include "trace-on.h"

static tlClass _tlFrameClass;
tlClass* tlFrameClass = &_tlFrameClass;

void* tlFrameAlloc(tlTask* task, tlResumeCb cb, size_t bytes) {
    tlFrame* frame = tlAlloc(task, tlFrameClass, bytes);
    frame->resumecb = cb;
    return frame;
}

tlFrame* tlFrameSetResume(tlTask* task, tlFrame* frame, tlResumeCb cb) {
    frame->resumecb = cb;
    return frame;
}

tlValue tlTaskPauseAttach(tlTask* task, void* frame);

