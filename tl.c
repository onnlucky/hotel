// starting point of hotel

#include "tl.h"
#include "debug.h"

int main(int argc, char** argv) {
    tl_init();

    const char* boot = "boot.tl";
    tlArgs* args = null;

    if (argc >= 2 && !strcmp(argv[1], "--noboot")) {
        if (argc < 3) fatal("no file to run");
        boot = argv[2];

        args = tlArgsNew(null, tlListNew(null, argc - 3), null);
        for (int i = 3; i < argc; i++) {
            tlArgsSet_(args, i - 3, tlTEXT(argv[i]));
        }
    } else {
        args = tlArgsNew(null, tlListNew(null, argc - 1), null);
        for (int i = 1; i < argc; i++) {
            tlArgsSet_(args, i - 1, tlTEXT(argv[i]));
        }
    }

    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    tlTask* maintask = tlVmEvalFile(vm, tlTEXT(boot), args);

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

