// author: Onne Gorter, license: MIT (see license.txt)

// evaluation can pause at any time, this is how we capture those pauses

#include "platform.h"
#include "frame.h"

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

void frame_init() {
    INIT_KIND(tlFrameKind);
}


