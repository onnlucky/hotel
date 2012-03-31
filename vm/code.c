// hotel "bytecode"

#include "trace-off.h"

static tlClass _tlCodeClass = { .name = "Code" };
tlClass* tlCodeClass = &_tlCodeClass;

struct tlCode {
    tlHead head;
    tlInt flags;
    tlText* file;
    tlInt line;
    tlSym name;
    tlList* argnames;
    tlMap* argdefaults;
    tlValue ops[];
};

void debugcode(tlCode* code);

tlCode* tlCodeNew(int size) {
    return tlAllocWithFields(tlCodeClass, sizeof(tlCode), size);
}
tlCode* tlCodeFrom(tlList* ops, tlText* file, tlInt line) {
    int size = tlListSize(ops);
    tlCode* code = tlCodeNew(size);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlListGet(ops, i);
    }
    code->file = file;
    code->line = line;
    if_trace(debugcode(code));
    return code;
}
void tlCodeSetIsBlock_(tlCode* code, bool isblock) {
    code->flags = tlOne;
}
bool tlCodeIsBlock(tlCode* code) {
    return code->flags == tlOne;
}
void tlCodeSetInfo_(tlCode* code, tlText* file, tlInt line, tlSym name) {
    assert(!code->name);
    code->name = name;
    if (!code->file) code->file = file;
    if (!code->line )code->line = line;
}
/*
void tlCodeSetOps_(tlCode* code, tlList* ops) {
    int size = tlListSize(ops);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlListGet(ops, i);
    }
}
*/
// TODO filter out collects ...
void tlCodeSetArgs_(tlCode* code, tlList* args) {
    trace("%d", tllist_size(args));
    // args = [name, default]*
    // output: name*, {name->default}

    int size = tlListSize(args);
    tlMap* defaults = tlMapFromPairs(args);

    tlList* names = tlListNew(size);
    for (int i = 0; i < size; i++) {
        tlList* entry = tlListAs(tlListGet(args, i));
        tlSym name = tlSymAs(tlListGet(entry, 0));
        tlListSet_(names, i, name);
    }
    code->argnames = names;
    code->argdefaults = defaults;
}
void tlCodeSetArgNameDefaults_(tlCode* code, tlList* name_defaults) {
    int size = tlListSize(name_defaults);
    tlList* names = tlListNew(size / 2);
    //tlList* defaults = tllist_new(size / 2);
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

void debugcall(int depth, tlCall* call) {
    fprintf(stderr, "%d, (", tl_head(call)->size);
    for (int i = 0; i < tl_head(call)->size; i++) {
        tlValue a = tlCallValueIter(call, i);
        if (i > 0) fprintf(stderr, ", ");
        fprintf(stderr, "%s", tl_str(a));
    }
    fprintf(stderr, ")\n");
}

void debugcode(tlCode* code) {
    print("size: %d", code->head.size);
    print("name: %s", tl_str(code->name));
    print("argnames: %p", code->argnames);
    print("argdefaults: %p", code->argdefaults);
    for (int i = 0; i < code->head.size; i++) {
        tlValue op = code->ops[i];
        print("%3d: %s%s", i, tlActiveIs(op)?"!":"", tl_str(op));
        if (tlActiveIs(op)) {
            op = tl_value(op);
            if (tlCallIs(op)) {
                debugcall(0, tlCallAs(op));
            }
        }
    }
    //fatal("DONE");
}

