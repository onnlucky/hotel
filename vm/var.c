// a mutable variable, basically a locked object but no queue since all operations are atomic

#include "trace-off.h"

static tlKind _tlVarKind = { .name = "Var" };
tlKind* tlVarKind = &_tlVarKind;

struct tlVar {
    tlHead head;
    tlHandle value;
};

tlHandle tlVarGet(tlVar* var) { return var->value; }
tlHandle tlVarSet(tlVar* var, tlHandle v) { var->value = v; return v; }

INTERNAL tlHandle _Var_new(tlArgs* args) {
    trace("");
    tlVar* var = tlAlloc(tlVarKind, sizeof(tlVar));
    var->value = tlArgsGet(args, 0);
    return var;
}
INTERNAL tlHandle __var_get(tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    if (!var->value) return tlNull;
    return var->value;
}
INTERNAL tlHandle __var_set(tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    var->value = tlArgsGet(args, 1);
    if (!var->value) return tlNull;
    return var->value;
}
INTERNAL tlHandle _var_get(tlArgs* args) {
    tlVar* var = tlVarAs(tlArgsTarget(args));
    tlHandle v = var->value;
    return v?v : tlNull;
}
INTERNAL tlHandle _var_set(tlArgs* args) {
    tlVar* var = tlVarAs(tlArgsTarget(args));
    tlHandle v = tlArgsGet(args, 0);
    var->value = v;
    return v?v : tlNull;
}

static const tlNativeCbs __var_natives[] = {
    { "_Var_new", _Var_new },
    { "_var_get", __var_get },
    { "_var_set", __var_set },
    { 0, 0 }
};

static tlMap* varClass;
static void var_init() {
    _tlVarKind.klass = tlClassMapFrom(
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

