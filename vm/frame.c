// evaluation can pause at any time, this is how we capture those pauses

#include "trace-off.h"

static tlClass _tlFrameClass;
tlClass* tlFrameClass = &_tlFrameClass;

void* tlFrameAlloc(tlResumeCb cb, size_t bytes) {
    tlFrame* frame = tlAlloc(tlFrameClass, bytes);
    frame->resumecb = cb;
    return frame;
}

tlFrame* tlFrameSetResume(tlFrame* frame, tlResumeCb cb) {
    frame->resumecb = cb;
    return frame;
}

tlValue tlTaskPauseAttach(void* frame);
tlValue tlTaskPauseResuming(tlResumeCb cb, tlValue res);
INTERNAL tlValue tlTaskRunThrow(tlTask* task, tlValue throw);

INTERNAL tlValue resumeCode(tlFrame* _frame, tlValue res, tlValue throw);
INTERNAL tlValue stopCode(tlFrame* _frame, tlValue res, tlValue throw);
static bool CodeFrameIs(tlFrame* frame) {
    return frame && (frame->resumecb == resumeCode || frame->resumecb == stopCode);
}

INTERNAL void tlFrameGetInfo(tlFrame* frame, tlText** file, tlText** function, tlInt* line);

