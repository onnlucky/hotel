
#ifndef HAVE_DEBUG
#else
#endif

void tlDumpTaskTrace();

static bool didbacktrace = false;
static bool doabort = true;
void tlabort() {
    if (didbacktrace) return;
    didbacktrace = true;

    void *bt[128];
    char **strings;
    size_t size;

    size = backtrace(bt, 128);
    strings = backtrace_symbols(bt, size);

    fprintf(stderr, "\nfatal error; backtrace:\n");
    for (int i = 0; i < size; i++) {
        fprintf(stderr, "%s\n", strings[i]);
    }
    fflush(stderr);

    tlDumpTaskTrace();

    if (doabort) {
        doabort = false;
        abort();
    }
}

void tlbacktrace_fatal(int x) {
    doabort = false;
    tlabort();
}

