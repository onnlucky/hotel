#include "tl.h"
#include "bcode.c"
#include "debug.h"

int main(int argc, char** argv) {
    tl_init();

#if 1
    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    tlTask* task = tlTaskNew(vm, tlMapEmpty());

    tlBModule* mod = tlBModuleNew(tlSTR("test-input"));
    tlHandle v = deserialize(tlBufferFromFile("test-input.tlb"), mod);
    tlBModuleLink(mod, tlVmGlobalEnv(vm));
    pprint(v);
    beval(task, mod);
    return 0;

#else

    const char* boot = null;
    tlArgs* args = null;

    if (argc >= 2 && !strcmp(argv[1], "--noboot")) {
        if (argc < 3) fatal("no file to run");
        boot = argv[2];

        args = tlArgsNew(tlListNew(argc - 3), null);
        for (int i = 3; i < argc; i++) {
            tlArgsSet_(args, i - 3, tlSTR(argv[i]));
        }
    } else {
        args = tlArgsNew(tlListNew(argc - 1), null);
        for (int i = 1; i < argc; i++) {
            tlArgsSet_(args, i - 1, tlSTR(argv[i]));
        }
    }

    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    if (boot) {
        tlTask* task = tlVmEvalFile(vm, tlSTR(boot), args);
        tlHandle h = tlTaskGetValue(task);
        if (tl_bool(h)) printf("%s\n", tl_str(h));
    } else {
        tlVmEvalBoot(vm, args);
    }

    int exitcode = tlVmExitCode(vm);
    tlVmDelete(vm);
    return exitcode;
#endif
}

