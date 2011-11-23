// evaluation can pause at any time, this is how we capture those pauses

#include "trace-on.h"

static tlClass _tlFrameClass;
tlClass* tlFrameClass = &_tlFrameClass;

// any hotel "operation" can be paused and resumed
// Any state that needs to be saved needs to be captured in a tlFrame
// all frames together form a call stack
struct tlFrame {
    tlHead head;
    tlFrame* caller;     // the frame below/after us
    tlResumeCb resumecb; // the resume function to be called when this frame is to resume
};

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

