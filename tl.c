// starting point of hotel

#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

// this is how a print function could look
static tlValue _print(tlTask* task, tlArgs* args, tlRun* run) {
    tlText* sep = tlTEXT(" ");
    tlValue v = tlargs_map_get(args, tlSYM("sep"));
    if (v) sep = tlvalue_to_text(task, v);

    tlText* begin = null;
    v = tlargs_map_get(args, tlSYM("begin"));
    if (v) begin = tlvalue_to_text(task, v);

    tlText* end = null;
    v = tlargs_map_get(args, tlSYM("end"));
    if (v) end = tlvalue_to_text(task, v);

    if (begin) printf("%s", tltext_bytes(begin));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlargs_get(args, i);
        if (!v) break;
        if (i > 0) printf("%s", tltext_bytes(sep));
        printf("%s", tltext_bytes(tlvalue_to_text(task, v)));
    }
    if (end) printf("%s", tltext_bytes(end));
    printf("\n");
    fflush(stdout);
    return tlNull;
}

// this is how to setup a vm
int main(int argc, char** argv) {
    tlvm_init();

    if (argc < 2) fatal("no file to run");
    const char* file = argv[1];
    tlBuffer* buf = tlbuffer_new_from_file(file);
    if (!buf) fatal("cannot read file: %s", file);

    tlbuffer_write_uint8(buf, 0);
    tlText* script = tltext_from_take(null, tlbuffer_free_get(buf));
    assert(script);

    tlVm* vm = tlvm_new();
    tlTask* task = tlvm_create_task(vm);

    tlEnv* env = tlvm_global_env(vm);
    tlFun* f_print = tlFUN(_print, tlSYM("print"));
    env = tlenv_set(null, env, tlSYM("print"), f_print);

    tlCode* code = tlcode_cast(parse(script));
    trace("PARSED");
    assert(code);

    tlClosure* fn = tlclosure_new(null, code, env);
    tltask_call(task, tlcall_from(null, fn, null));

    trace("STEPPING");
    while (task->run) tltask_step(task);

    trace("DONE");
    tlValue v = tltask_value(task);
    if (tl_bool(v)) printf("%s\n", tl_str(v));
    tlvm_delete(vm);
    return 0;
}

