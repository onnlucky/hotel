// evaluation can pause at any time, this is how we capture those pauses

#include "trace-off.h"

static tlKind _tlFrameKind = {
    .name = "Frame"
};
tlKind* tlFrameKind;

void* tlFrameAlloc(tlResumeCb cb, size_t bytes) {
    tlFrame* frame = tlAlloc(tlFrameKind, bytes);
    frame->resumecb = cb;
    return frame;
}

tlFrame* tlFrameSetResume(tlFrame* frame, tlResumeCb cb) {
    frame->resumecb = cb;
    return frame;
}

INTERNAL void tlFrameGetInfo(tlFrame* frame, tlString** file, tlString** function, tlInt* line);

