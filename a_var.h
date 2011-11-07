// hotel atomic variables and values
#pragma once

// TODO actually implement using atomic operations and barriers ... sigh

// atomic values (a_val) must be pointer sized
// if the value is a *pointer* use the A_VAL and A_PTR to convert to and from a_val
// both will do the appriopriate read/write barrier
// use the A_VAL_NB or A_PTR_NB to skip the barrier ...
typedef intptr_t a_val;
typedef a_val* a_var;

static inline void check_var(void* ptr) { }
#define A_VAR(x) (check_var(x), (a_var)(&x))
static inline a_val A_VAL_NB(void* ptr) { return (intptr_t)ptr; }
static inline a_val A_VAL(void* ptr) {
    // TODO do a write barrier
    return (intptr_t)ptr;
}
static inline void* A_PTR_NB(a_val v) { return (void*)v; }
static inline void* A_PTR(a_val v) {
    // TODO do a read barrier
    return (void*)v;
}

// get the value from a atomic variable
static inline a_val a_get(a_var var) {
    return *var;
}

// set the value of a atomic variable
// returns the new value of the variable
static inline a_val a_set(a_var var, a_val v) {
    return *var = v;
}
// set the value of a atomic variable only if the current value is something specific
// returns the new value of the variable
static inline a_val a_set_if(a_var var, a_val v, a_val current) {
    if (*var == current) return *var = v;
    return *var;
}

// sets the value of a atomic variable, but returns the old value
static inline a_val a_swap(a_var var, a_val v) {
    a_val old = *var; *var = v; return old;
}
// sets the value of a atomic variable if the current value is something specific
// retuns the old value (which per definition is the third argument)
static inline a_val a_swap_if(a_var var, a_val v, a_val current) {
    a_val old = *var;
    if (old == current) { *var = v; return old; }
    return v;
}

