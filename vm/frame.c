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
tlValue tlTaskPauseResuming(tlTask* task, tlResumeCb cb, tlValue res);
INTERNAL tlValue tlTaskRunThrow(tlTask* task, tlValue throw);

INTERNAL tlValue resumeCode(tlTask* task, tlFrame* _frame, tlValue res, tlValue throw);
INTERNAL tlValue stopCode(tlTask* task, tlFrame* _frame, tlValue res, tlValue throw);
static bool CodeFrameIs(tlFrame* frame) {
    return frame && (frame->resumecb == resumeCode || frame->resumecb == stopCode);
}

INTERNAL void tlFrameGetInfo(tlFrame* frame, tlText** file, tlText** function, tlInt* line);

