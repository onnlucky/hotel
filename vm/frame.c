// evaluation can pause at any time, this is how we capture those pauses

#include "trace-off.h"

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

INTERNAL tlValue resumeCode(tlTask* task, tlFrame* _frame, tlValue res, tlError* err);
INTERNAL tlValue stopCode(tlTask* task, tlFrame* _frame, tlValue res, tlError* err);
static bool CodeFrameIs(tlFrame* frame) {
    return frame && (frame->resumecb == resumeCode || frame->resumecb == stopCode);
}

