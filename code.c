// hotel "bytecode"

#include "trace-off.h"

struct tlCode {
    tlHead head;
    tlInt flags;
    tlSym name;
    tlList* argnames;
    tlMap* argdefaults;
    tlValue ops[];
};

void debugcode(tlCode* code);

tlCode* tlcode_new(tlTask* task, int size) {
    return task_alloc(task, TLCode, 4 + size);
}
tlCode* tlcode_from(tlTask* task, tlList* ops) {
    int size = tlListSize(ops);
    tlCode* code = tlcode_new(task, size);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlListGet(ops, i);
    }
    //debugcode(code);
    return code;
}
void tlcode_set_isblock_(tlCode* code, bool isblock) {
    code->flags = tlOne;
}
bool tlcode_isblock(tlCode* code) {
    return code->flags == tlOne;
}
void tlcode_set_name_(tlCode* code, tlSym name) {
    code->name = name;
}
void tlcode_set_ops_(tlCode* code, tlList* ops) {
    int size = tlListSize(ops);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlListGet(ops, i);
    }
}
// TODO filter out collects ...
void tlcode_set_args_(tlTask* task, tlCode* code, tlList* args) {
    trace("%d", tllist_size(args));
    // args = [name, default]*
    // output: name*, {name->default}

    int size = tlListSize(args);
    tlMap* defaults = tlmap_from_pairs(task, args);

    tlList* names = tlListNew(task, size);
    for (int i = 0; i < size; i++) {
        tlList* entry = tlListAs(tlListGet(args, i));
        tlSym name = tlsym_as(tlListGet(entry, 0));
        tlListSet_(names, i, name);
    }
    code->argnames = names;
    code->argdefaults = defaults;
}
void tlcode_set_arg_name_defaults_(tlTask* task, tlCode* code, tlList* name_defaults) {
    int size = tlListSize(name_defaults);
    tlList* names = tlListNew(task, size / 2);
    //tlList* defaults = tllist_new(task, size / 2);
    assert(size % 2 == 0);

    bool have_defaults = false;
    for (int i = 0; i < size; i += 2) {
        tlListSet_(names, i / 2, tlListGet(name_defaults, i));
        tlValue v = tlListGet(name_defaults, i + 1);
        if (v) have_defaults = true;
        tlListSet_(names, i / 2, v);
    }
    code->argnames = names;
    if (have_defaults) {
        //code->argdefaults = defaults;
    }
}

void debugcode(tlCode* code) {
    print("size: %d", code->head.size);
    print("name: %s", tl_str(code->name));
    print("argnames: %p", code->argnames);
    print("argdefaults: %p", code->argdefaults);
    for (int i = 0; i < code->head.size - 4; i++) {
        print("%3d: %s%s", i, tlactive_is(code->ops[i])?"!":"", tl_str(code->ops[i]));
    }
}

