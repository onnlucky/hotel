#include "tl.h"
#include "debug.h"

extern tlBModule* g_boot_module;

int main(int argc, char** argv) {
    tl_init();

    const char* boot = null;
    tlArgs* args = null;

    if (argc >= 2 && !strcmp(argv[1], "--boot")) {
        if (argc < 3) fatal("no boot file");
        boot = argv[2];

        args = tlArgsNewNew(argc - 3);
        for (int i = 3; i < argc; i++) {
            tlArgsSet_(args, i - 3, tlSTR(argv[i]));
        }
    } else {
        args = tlArgsNewNew(argc - 1);
        for (int i = 1; i < argc; i++) {
            tlArgsSet_(args, i - 1, tlSTR(argv[i]));
        }
    }

    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    tlBuffer* buf = null;
    if (boot) {
        buf = tlBufferFromFile(boot);
    } else {
        boot = "boot.tlb";
        buf = tlBufferFromFile("boot/boot.tlb");
        if (!buf) buf = tlBufferFromFile("../boot/boot.tlb");
        if (!buf) buf = tlBufferFromFile("../../boot/boot.tlb");
        if (!buf) buf = tlBufferFromFile("/usr/shared/tl/boot.tlb");
    }
    if (!buf) fatal("unable to load: %s", boot);

    const char* error = null;
    tlBModule* mod = tlBModuleFromBuffer(buf, tlSTR(boot), &error);
    if (error) fatal("error booting: %s", error);
    g_boot_module = mod;

    tlBModuleLink(mod, tlVmGlobalEnv(vm), &error);
    if (error) fatal("error linking boot: %s", error);

    tlTask* task = tlBModuleCreateTask(vm, mod, args);
    tlVmRun(vm, task);
    tlHandle h = tlTaskGetValue(task);
    if (tl_bool(h)) printf("%s\n", tl_str(h));

    int exitcode = tlVmExitCode(vm);
    tlVmDelete(vm);
    return exitcode;
}

