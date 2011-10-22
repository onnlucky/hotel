// evaluation can pause at any time, this is how we capture those pauses
// TODO rename tlPause to tlFrame

#include "trace-on.h"

static tlClass _tlPauseClass;
tlClass* tlPauseClass = &_tlPauseClass;

// any hotel "operation" can be paused and resumed using a tlPause object
// a Pause is basically the equivalent of a continuation or a stack frame
// notice, most operations will only materialize a pause if they need to
struct tlPause {
    tlHead head;
    tlPause* caller;     // the pause below/after us
    tlResumeCb resumecb; // a function called when resuming
};

void* tlPauseAlloc(tlTask* task, size_t bytes, int fields, tlResumeCb cb) {
    tlPause* pause = tlAlloc(task, tlPauseClass, bytes + fields * sizeof(tlValue));
    // TODO for now ...
    pause->head.type = TLPause;
    pause->resumecb = cb;
    return pause;
}

tlPause* tlTaskPauseAttach(tlTask*, tlPause*, void*);

