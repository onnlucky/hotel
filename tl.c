// starting point of hotel

#include "tl.h"
#include "debug.h"

int main(int argc, char** argv) {
    tl_init();

    if (argc < 2) fatal("no file to run");

    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    tlTask* maintask = tlVmRunFile(vm, tlTEXT(argv[1]));

    tlValue err = tlTaskGetError(maintask);
    if (err) {
        printf("%s\n", tl_str(err));
        return 1;
    }
    tlValue v = tlTaskGetValue(maintask);
    if (tl_bool(v)) printf("%s\n", tl_str(v));
    tlVmDelete(vm);
    return 0;
}

