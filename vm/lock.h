#ifndef _lock_h_
#define _lock_h_

#include "tl.h"

bool tlLockIsOwner(tlLock* lock, tlTask* task);
tlHandle tlLockAndInvoke(tlTask* task, tlArgs* call);

#endif
