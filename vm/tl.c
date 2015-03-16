// author: Onne Gorter, license: MIT (see license.txt)

// the is the entrypoint that runs tl scripts

// use TL_MODULE_PATH to change where it will look for modules, a ':' separated path
// e.g. TL_MODULE_PATH=/usr/local/lib/tl:/home/username/.tlmodules/

#include "platform.h"
#include "tl.h"

extern tlBModule* g_init_module;

int main(int argc, char** argv) {
    tl_init();

    const char* init = null;
    tlArgs* args = null;

    if (argc >= 2 && !strcmp(argv[1], "--version")) {
        printf("hotel language interpreter, version " TL_VERSION "\n");
        return 0;
    }

    if (argc >= 2 && !strcmp(argv[1], "--init")) {
        if (argc < 3) fatal("no init file");
        init = argv[2];

        args = tlArgsNew(argc - 3);
        for (int i = 3; i < argc; i++) {
            tlArgsSet_(args, i - 3, tlSTR(argv[i]));
        }
    } else {
        args = tlArgsNew(argc - 1);
        for (int i = 1; i < argc; i++) {
            tlArgsSet_(args, i - 1, tlSTR(argv[i]));
        }
    }

    tlVm* vm = tlVmNew(tlSYM(argv[0]), args);
    tlVmInitDefaultEnv(vm);
    tlBuffer* buf = null;
    if (init) {
        buf = tlBufferFromFile(init);
    } else {
        init = "<init>";
        buf = tlVmGetInit();
        assert(buf);
    }
    if (!buf) fatal("unable to load: %s", init);

    tlTask* task = tlVmEvalInitBuffer(vm, buf, init, args);
    tlHandle h = tlTaskValue(task);
    if (tlTaskHasError(task)) {
        if (tlStringIs(h)) {
            printf("Error: %s\n", tl_str(h));
        } else {
            printf("Error: %s\n", tl_repr(h));
        }
    } else {
        if (tlStringIs(h)) {
            printf("%s\n", tl_str(h));
        } else if (tl_bool(h)) {
            if (tl_bool(h)) printf("%s\n", tl_repr(h));
        }
    }

    int exitcode = tlVmExitCode(vm);
    tlVmDelete(vm);
    return exitcode;
}

