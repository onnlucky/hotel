// hotel "bytecode"

#include "trace-on.h"

struct tCode {
    tHead head;
    tInt flags;
    tSym name;
    tList* argnames;
    tMap* argdefaults;
    tValue ops[];
};
TTYPE(tCode, tcode, TCode);

tCode* tcode_new(tTask* task, int size) {
    return task_alloc(task, TCode, 4 + size);
}
tCode* tcode_from(tTask* task, tList* ops) {
    int size = tlist_size(ops);
    tCode* code = tcode_new(task, size);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlist_get(ops, i);
    }
    return code;
}
void tcode_set_name_(tCode* code, tSym name) {
    code->name = name;
}
void tcode_set_ops_(tCode* code, tList* ops) {
    int size = tlist_size(ops);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlist_get(ops, i);
    }
}
// TODO filter out collects ...
void tcode_set_args_(tTask* task, tCode* code, tList* args) {
    trace("%d", tlist_size(args));
    // args = [name, default]*
    // output: name*, {name->default}

    int size = tlist_size(args);
    tMap* defaults = tmap_from_pairs(task, args);

    tList* names = tlist_new(task, size);
    for (int i = 0; i < size; i++) {
        tList* entry = tlist_as(tlist_get(args, i));
        tSym name = tsym_as(tlist_get(entry, 0));
        tlist_set_(names, i, name);
    }
    code->argnames = names;
    code->argdefaults = defaults;
}

void tcode_set_arg_name_defaults_(tTask* task, tCode* code, tList* name_defaults) {
    int size = tlist_size(name_defaults);
    tList* names = tlist_new(task, size / 2);
    tList* defaults = tlist_new(task, size / 2);
    assert(size % 2 == 0);

    bool have_defaults = false;
    for (int i = 0; i < size; i += 2) {
        tlist_set_(names, i / 2, tlist_get(name_defaults, i));
        tValue v = tlist_get(name_defaults, i + 1);
        if (v) have_defaults = true;
        tlist_set_(names, i / 2, v);
    }
    code->argnames = names;
    if (have_defaults) {
        code->argdefaults = defaults;
    }
}

