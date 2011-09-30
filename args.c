struct tlArgs {
    tlHead head;
    tlValue fn;
    tlValue target;
    tlValue msg;
    tlMap* map;
    tlList* list;
};

static tlArgs* v_args_empty;

tlArgs* tlargs_new_names(tlTask* task, int size, tlSet* names) {
    if (!names) names = v_set_empty;
    tlArgs* args = task_alloc(task, TLArgs, 5);
    args->list = tllist_new(task, size - tlset_size(names));
    args->map = tlmap_new(task, names);
    return args;
}
tlArgs* tlargs_new(tlTask* task, tlList* list, tlMap* map) {
    if (!list) list = v_list_empty;
    if (!map) map = v_map_empty;
    tlArgs* args = task_alloc(task, TLArgs, 5);
    args->list = list;
    args->map = map;
    return args;
}

tlValue tlArgsTarget(tlArgs* args) { return args->target; }

int tlargs_size(tlArgs* args) {
    assert(tlargs_is(args));
    return tllist_size(args->list);
}
tlValue tlargs_fn(tlArgs* args) {
    assert(tlargs_is(args));
    return args->fn;
}
tlList* tlargs_list(tlArgs* args) {
    assert(tlargs_is(args));
    return args->list;
}
tlMap* tlargs_map(tlArgs* args) {
    assert(tlargs_is(args));
    return args->map;
}
tlValue tlargs_get(tlArgs* args, int at) {
    assert(tlargs_is(args));
    return tllist_get(args->list, at);
}
tlValue tlargs_map_get(tlArgs* args, tlSym name) {
    assert(tlargs_is(args));
    return tlmap_get_sym(args->map, name);
}

void tlargs_fn_set_(tlArgs* args, tlValue fn) {
    assert(tlargs_is(args));
    assert(!args->fn);
    args->fn = fn;
}
void tlargs_set_(tlArgs* args, int at, tlValue v) {
    assert(tlargs_is(args));
    tllist_set_(args->list, at, v);
}
void tlargs_map_set_(tlArgs* args, tlSym name, tlValue v) {
    assert(tlargs_is(args));
    tlmap_set_sym_(args->map, name, v);
}

static tlPause* _args_size(tlTask* task, tlArgs* args) {
    tlArgs* as = tlargs_cast(tlargs_get(args, 0));
    if (!as) TL_THROW("Expected a args object");
    TL_RETURN(tlINT(tlargs_size(as)));
}
static tlPause* _args_names(tlTask* task, tlArgs* args) {
    tlArgs* as = tlargs_cast(tlargs_get(args, 0));
    if (!as) TL_THROW("Expected a args object");
    TL_RETURN(tlargs_map(as));
}

static const tlHostCbs __args_hostcbs[] = {
    { "_args_size", _args_size },
    { "_args_names", _args_names },
    { 0, 0 }
};

void args_init() {
    v_args_empty = tlargs_new(null, null, null);
    tl_register_hostcbs(__args_hostcbs);
}

