#ifndef _task_h_
#define _task_h_

#include "tl.h"

bool tlTaskHasError(tlTask* task);
tlHandle tlTaskClearError(tlTask* task, tlHandle value);
tlHandle tlTaskError(tlTask* task, tlHandle error);

tlHandle tlTaskValue(tlTask* task);

tlFrame* tlTaskCurrentFrame(tlTask* task);
void tlTaskPushFrame(tlTask* task, tlFrame* frame);
void tlTaskPopFrame(tlTask* task, tlFrame* frame);

#endif
