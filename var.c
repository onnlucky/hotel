// a mutable variable, basically an actor but no queue since all operations are atomic

#include "trace-on.h"

static tlClass _tlVarClass = {
    .name = "Var"
};
tlClass* tlVarClass = &_tlVarClass;

struct tlVar {
    tlHead head;
    tlValue value;
};

INTERNAL tlValue _Var_new(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlAlloc(task, tlVarClass, sizeof(tlVar));
    var->value = tlArgsAt(args, 0);
    return var;
}
INTERNAL tlValue _var_get(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsAt(args, 0));
    if (!var) TL_THROW("expected a Var");
    if (!var->value) return tlNull;
    return var->value;
}
INTERNAL tlValue _var_set(tlTask* task, tlArgs* args) {
    trace("");
    tlVar* var = tlVarCast(tlArgsAt(args, 0));
    if (!var) TL_THROW("expected a Var");
    var->value = tlArgsAt(args, 1);
    if (!var->value) return tlNull;
    return var->value;
}

static const tlHostCbs __var_hostcbs[] = {
    { "_Var_new", _Var_new },
    { "_var_get", _var_get },
    { "_var_set", _var_set },
    { 0, 0 }
};

INTERNAL void var_init() {
    tl_register_hostcbs(__var_hostcbs);
}

