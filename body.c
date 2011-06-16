#include "trace-on.h"

struct tBody {
    tHead head;
    tInt flags;
    tSym name;
    tList* argnames;
    tMap* argdefaults;
    tValue ops[];
};
TTYPE(tBody, tbody, TBody);

tBody* tbody_new(tTask* task, int size) {
    return task_alloc(task, TBody, 4 + size);
}
tBody* tbody_from(tTask* task, tList* ops) {
    int size = tlist_size(ops);
    tBody* body = tbody_new(task, size);
    for (int i = 0; i < size; i++) {
        body->ops[i] = tlist_get(ops, i);
    }
    return body;
}
void tbody_set_name_(tBody* body, tSym name) {
    body->name = name;
}
void tbody_set_ops_(tBody* body, tList* ops) {
    int size = tlist_size(ops);
    for (int i = 0; i < size; i++) {
        body->ops[i] = tlist_get(ops, i);
    }
}
// TODO filter out collects ...
void tbody_set_args_(tTask* task, tBody* body, tList* args) {
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
    body->argnames = names;
    body->argdefaults = defaults;
}

void tbody_set_arg_name_defaults_(tTask* task, tBody* body, tList* name_defaults) {
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
    body->argnames = names;
    if (have_defaults) {
        body->argdefaults = defaults;
    }
}

