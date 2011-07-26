// starting point of hotel

#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

// this is how a print function could look
static tlRun* _print(tlTask* task, tlArgs* args) {
    tlText* sep = tlTEXT(" ");
    tlValue v = tlargs_map_get(args, tlSYM("sep"));
    if (v) sep = tlvalue_to_text(task, v);

    tlText* begin = null;
    v = tlargs_map_get(args, tlSYM("begin"));
    if (v) begin = tlvalue_to_text(task, v);

    tlText* end = null;
    v = tlargs_map_get(args, tlSYM("end"));
    if (v) end = tlvalue_to_text(task, v);

    if (begin) printf("%s", tltext_data(begin));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlargs_get(args, i);
        if (!v) break;
        if (i > 0) printf("%s", tltext_data(sep));
        printf("%s", tltext_data(tlvalue_to_text(task, v)));
    }
    if (end) printf("%s", tltext_data(end));
    printf("\n");
    fflush(stdout);
    TL_RETURN(tlNull);
}

static tlRun* _assert(tlTask* task, tlArgs* args) {
    tlText* text = tltext_cast(tlargs_map_get(args, tlSYM("text")));
    if (!text) text = tltext_empty();
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlargs_get(args, i);
        if (!v) break;
        if (!tl_bool(v)) {
            //text = tltask_cat(task, tlTEXT("Assertion Failed: "), text);
            TL_THROW("Assertion Failed");
        }
    }
    TL_RETURN(tlNull);
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
    tlWorker* worker = tlworker_new(vm);
    tlTask* task = tltask_new(worker);

    tlEnv* env = tlvm_global_env(vm);
    tlHostFn* f_print = tlhostfn_new(task, _print, 1);
    tlhostfn_set_(f_print, 0, tlSYM("print"));
    //assert(tlSYM("print") == tlhostfn_get(f_print, 0));
    env = tlenv_set(null, env, tlSYM("print"), f_print);

    tlHostFn* f_assert = tlhostfn_new(task, _assert, 1);
    tlhostfn_set_(f_assert, 0, tlSYM("assert"));
    env = tlenv_set(null, env, tlSYM("assert"), f_assert);

    tlCode* code = tlcode_cast(tl_parse(task, script));
    trace("PARSED");
    assert(code);

    tlClosure* fn = tlclosure_new(task, code, env);
    tltask_call(task, tlcall_from(task, fn, null));
    tltask_ready_detach(task);

    trace("RUNNING");
    tlworker_run(worker);
    trace("DONE");

    tlValue ex = tltask_exception(task);
    if (ex) {
        printf("%s\n", tl_str(ex));
        return 1;
    }

    tlValue v = tltask_value(task);
    if (tl_bool(v)) printf("%s\n", tl_str(v));
    tlvm_delete(vm);
    return 0;
}

