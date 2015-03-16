// author: Onne Gorter, license: MIT (see license.txt)

// a mutable variable, basically a locked object but no queue since all operations are atomic
// TODO remove, we now have local variables ...

#include "platform.h"
#include "var.h"

#include "value.h"

static tlKind _tlVarKind = { .name = "Var" };
tlKind* tlVarKind;

struct tlVar {
    tlHead head;
    tlHandle value;
};

tlHandle tlVarGet(tlVar* var) { return var->value; }
tlHandle tlVarSet(tlVar* var, tlHandle v) { var->value = v; return v; }

static tlHandle _Var_new(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlAlloc(tlVarKind, sizeof(tlVar));
    var->value = tlArgsGet(args, 0);
    return var;
}
static tlHandle __var_get(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    if (!var->value) return tlNull;
    return var->value;
}
static tlHandle __var_set(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsGet(args, 0));
    if (!var) TL_THROW("expected a Var");
    var->value = tlArgsGet(args, 1);
    if (!var->value) return tlNull;
    return var->value;
}
static tlHandle _var_get(tlTask* task, tlArgs* args) {
    tlVar* var = tlVarAs(tlArgsTarget(args));
    tlHandle v = var->value;
    return v?v : tlNull;
}
static tlHandle _var_set(tlTask* task, tlArgs* args) {
    tlVar* var = tlVarAs(tlArgsTarget(args));
    tlHandle v = tlArgsGet(args, 0);
    var->value = v;
    return v?v : tlNull;
}

static tlHandle _isVar(tlTask* task, tlArgs* args) {
    return tlBOOL(!!tlVarCast(tlArgsGet(args, 0)));
}

static const tlNativeCbs __var_natives[] = {
    { "_Var_new", _Var_new },
    { "_var_get", __var_get },
    { "_var_set", __var_set },
    { "isVar", _isVar },
    { 0, 0 }
};

static tlObject* varClass;

void var_init() {
    _tlVarKind.klass = tlClassObjectFrom(
        "get", _var_get,
        "set", _var_set,
        null
    );
    varClass = tlClassObjectFrom(
        "new", _Var_new,
        null
    );
    // TODO remove this in favor of Var.new etc ...
    tl_register_natives(__var_natives);

    INIT_KIND(tlVarKind);
}

void var_vm_default(tlVm* vm) {
   tlVmGlobalSet(vm, tlSYM("Var"), varClass);
}

