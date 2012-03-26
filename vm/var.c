// a mutable variable, basically a locked object but no queue since all operations are atomic

#include "trace-off.h"

static tlClass _tlVarClass = { .name = "Var" };
tlClass* tlVarClass = &_tlVarClass;

struct tlVar {
    tlHead head;
    tlValue value;
};

tlValue tlVarGet(tlVar* var) { return var->value; }
tlValue tlVarSet(tlVar* var, tlValue v) { var->value = v; return v; }

INTERNAL tlValue _Var_new(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlAlloc(task, tlVarClass, sizeof(tlVar));
    var->value = tlArgsGet(args, 0);
    return var;
}
INTERNAL tlValue _var_get(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    if (!var->value) return tlNull;
    return var->value;
}
INTERNAL tlValue _var_set(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    var->value = tlArgsGet(args, 1);
    if (!var->value) return tlNull;
    return var->value;
}

static const tlNativeCbs __var_natives[] = {
    { "_Var_new", _Var_new },
    { "_var_get", _var_get },
    { "_var_set", _var_set },
    { 0, 0 }
};

static tlMap* varClass;
static void var_init() {
    _tlVarClass.map = tlClassMapFrom(
        "get", _var_get,
        "set", _var_set,
        null
    );
    varClass = tlClassMapFrom(
        "new", _Var_new,
        null
    );
    // TODO remove this in favor of Var.new etc ...
    tl_register_natives(__var_natives);
}

static void var_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Var"), varClass);
}

