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
TTYPE(tlCode, tlcode, TLCode);

tlCode* tlcode_new(tlTask* task, int size) {
    return task_alloc(task, TLCode, 4 + size);
}
tlCode* tlcode_from(tlTask* task, tlList* ops) {
    int size = tllist_size(ops);
    tlCode* code = tlcode_new(task, size);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tllist_get(ops, i);
    }
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
    int size = tllist_size(ops);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tllist_get(ops, i);
    }
}
// TODO filter out collects ...
void tlcode_set_args_(tlTask* task, tlCode* code, tlList* args) {
    trace("%d", tllist_size(args));
    // args = [name, default]*
    // output: name*, {name->default}

    int size = tllist_size(args);
    tlMap* defaults = tlmap_from_pairs(task, args);

    tlList* names = tllist_new(task, size);
    for (int i = 0; i < size; i++) {
        tlList* entry = tllist_as(tllist_get(args, i));
        tlSym name = tlsym_as(tllist_get(entry, 0));
        tllist_set_(names, i, name);
    }
    code->argnames = names;
    code->argdefaults = defaults;
}
void tlcode_set_arg_name_defaults_(tlTask* task, tlCode* code, tlList* name_defaults) {
    int size = tllist_size(name_defaults);
    tlList* names = tllist_new(task, size / 2);
    //tlList* defaults = tllist_new(task, size / 2);
    assert(size % 2 == 0);

    bool have_defaults = false;
    for (int i = 0; i < size; i += 2) {
        tllist_set_(names, i / 2, tllist_get(name_defaults, i));
        tlValue v = tllist_get(name_defaults, i + 1);
        if (v) have_defaults = true;
        tllist_set_(names, i / 2, v);
    }
    code->argnames = names;
    if (have_defaults) {
        //code->argdefaults = defaults;
    }
}

