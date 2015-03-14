#ifndef _tlregex_h_
#define _tlregex_h_

#include <regex.h>

#include "tl.h"

TL_REF_TYPE(tlRegex);
struct tlRegex {
    tlHead head;
    regex_t compiled;
};

tlHandle _isRegex(tlTask* task, tlArgs* args);

void regex_init_vm(tlVm* vm);

#endif
