// evaluation can pause at any time, this is how we capture those pauses

#include "trace-off.h"

static tlKind _tlFrameKind;
tlKind* tlFrameKind = &_tlFrameKind;

void* tlFrameAlloc(tlResumeCb cb, size_t bytes) {
    tlFrame* frame = tlAlloc(tlFrameKind, bytes);
    frame->resumecb = cb;
    return frame;
}

tlFrame* tlFrameSetResume(tlFrame* frame, tlResumeCb cb) {
    frame->resumecb = cb;
    return frame;
}

tlHandle tlTaskPauseAttach(void* frame);
tlHandle tlTaskPauseResuming(tlResumeCb cb, tlHandle res);
INTERNAL tlHandle tlTaskRunThrow(tlTask* task, tlHandle throw);

INTERNAL tlHandle resumeCode(tlFrame* _frame, tlHandle res, tlHandle throw);
INTERNAL tlHandle stopCode(tlFrame* _frame, tlHandle res, tlHandle throw);
static bool tlCodeFrameIs(tlFrame* frame) {
    return frame && (frame->resumecb == resumeCode || frame->resumecb == stopCode);
}

INTERNAL void tlFrameGetInfo(tlFrame* frame, tlText** file, tlText** function, tlInt* line);

