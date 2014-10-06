// hotel "bytecode"

#include "trace-off.h"

static tlKind _tlCodeKind = { .name = "Code" };
tlKind* tlCodeKind;

struct tlCode {
    tlHead head;
    tlInt flags;
    tlString* file;
    tlString* path;
    tlInt line;
    tlSym name;
    tlList* argnames;
    tlObject* argdefaults;
    intptr_t size;
    tlHandle ops[];
};

void debugcode(tlCode* code);

tlCode* tlCodeNew(int size) {
    tlCode* code = tlAlloc(tlCodeKind, sizeof(tlCode) + sizeof(tlHandle) * size);
    code->size = size;
    return code;
}
tlCode* tlCodeFrom(tlList* ops, tlString* file, tlInt line) {
    int size = tlListSize(ops);
    tlCode* code = tlCodeNew(size);
    for (int i = 0; i < size; i++) {
        code->ops[i] = tlListGet(ops, i);
    }
    if (file) {
        code->file = file;
        code->path = tlStringFromCopy(dirname(strdup(tlStringData(file))), 0);
    } else {
        code->file = tlNull;
        code->path = tlNull;
    }
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
void tlCodeSetInfo_(tlCode* code, tlString* file, tlInt line, tlSym name) {
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
    trace("%d", tlListSize(args));
    // args = [name, default]*
    // output: name*, {name->default}

    int size = tlListSize(args);
    tlObject* defaults = tlObjectFromPairs(args);

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
        tlHandle v = tlListGet(name_defaults, i + 1);
        if (v) have_defaults = true;
        tlListSet_(names, i / 2, v);
    }
    code->argnames = names;
    if (have_defaults) {
        //code->argdefaults = defaults;
    }
}

