#ifndef _error_h_
#define _error_h_

#include "tl.h"

TL_REF_TYPE(tlStackTrace);
struct tlStackTrace {
    tlHead head;
    intptr_t size;
    tlTask* task;
    tlHandle entries[];
    // [tlString, tlString, tlInt, ...] so size * 3 is entries.length
};

tlStackTrace* tlStackTraceNew(tlTask* task, tlFrame* stack, int skip);

tlHandle tlDeadlockErrorThrow(tlTask* task, tlArray* tasks);

void tlErrorAttachStack(tlTask* task, tlHandle _err, tlFrame* frame);

void error_init();
void error_vm_default(tlVm* vm);

#endif
