// starting point of hotel

#include "tl.h"

#include "vm.c"

#include "debug.h"
#include "trace-on.h"

// this is how a print function could look
static tlValue _print(tlTask* task, tlArgs* args) {
    tlText* sep = tlTEXT(" ");
    tlValue v = tlArgsMapGet(args, tlSYM("sep"));
    if (v) sep = tlvalue_to_text(task, v);

    tlText* begin = null;
    v = tlArgsMapGet(args, tlSYM("begin"));
    if (v) begin = tlvalue_to_text(task, v);

    tlText* end = null;
    v = tlArgsMapGet(args, tlSYM("end"));
    if (v) end = tlvalue_to_text(task, v);

    if (begin) printf("%s", tlTextData(begin));
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlArgsAt(args, i);
        if (!v) break;
        if (i > 0) printf("%s", tlTextData(sep));
        printf("%s", tlTextData(tlvalue_to_text(task, v)));
    }
    if (end) printf("%s", tlTextData(end));
    printf("\n");
    fflush(stdout);
    return tlNull;
}

static tlValue _assert(tlTask* task, tlArgs* args) {
    tlText* text = tlTextCast(tlArgsMapGet(args, tlSYM("text")));
    if (!text) text = tlTextEmpty();
    for (int i = 0; i < 1000; i++) {
        tlValue v = tlArgsAt(args, i);
        if (!v) break;
        if (!tl_bool(v)) {
            //text = tltask_cat(task, tlTEXT("Assertion Failed: "), text);
            TL_THROW("Assertion Failed");
        }
    }
    return tlNull;
}

// this is how to setup a vm
int main(int argc, char** argv) {
    tlvm_init();

    if (argc < 2) fatal("no file to run");
    const char* file = argv[1];
    tl_buf* buf = tlbuf_new_from_file(file);
    if (!buf) fatal("cannot read file: %s", file);

    tlbuf_write_uint8(buf, 0);
    tlText* script = tlTextNewTake(null, tlbuf_free_get(buf));
    assert(script);

    tlVm* vm = tlvm_new();
    tlWorker* worker = tlworker_new(vm);
    tlTask* task = tlTaskNew(worker);

    tlEnv* env = tlvm_global_env(vm);
    tlHostFn* f_print = tlHostFnNew(task, _print, 1);
    tlHostFnSet_(f_print, 0, tlSYM("print"));
    //assert(tlSYM("print") == tlHostFnGet(f_print, 0));
    env = tlenv_set(null, env, tlSYM("print"), f_print);

    tlHostFn* f_assert = tlHostFnNew(task, _assert, 1);
    tlHostFnSet_(f_assert, 0, tlSYM("assert"));
    env = tlenv_set(null, env, tlSYM("assert"), f_assert);

    tlCode* code = tlcode_cast(tl_parse(task, script));
    trace("PARSED");
    assert(code);

    tlClosure* fn = tlclosure_new(task, code, env);
    tlCall* call = tlcall_from(task, fn, null);
    tlTaskEval(task, call);
    tlTaskReadyInit(task);

    trace("RUNNING");
    tlworker_run_io(worker);
    trace("DONE");

    tlValue err = tlTaskGetError(task);
    if (err) {
        printf("%s\n", tl_str(err));
        return 1;
    }

    tlValue v = tlTaskGetValue(task);
    if (tl_bool(v)) printf("%s\n", tl_str(v));
    tlvm_delete(vm);
    return 0;
}

