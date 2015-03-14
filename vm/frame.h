#ifndef _frame_h_
#define _frame_h_

#include "tl.h"

void* tlFrameAlloc(tlResumeCb cb, size_t bytes);
void tlFrameGetInfo(tlFrame* frame, tlString** file, tlString** function, tlInt* line);

void frame_init();

#endif
