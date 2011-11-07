// hotel atomic variables and values
#pragma once

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
    __sync_synchronize();
    return (intptr_t)ptr;
}
static inline void* A_PTR_NB(a_val v) { return (void*)v; }
static inline void* A_PTR(a_val v) {
    __sync_synchronize();
    return (void*)v;
}

// get the value from a atomic variable
static inline a_val a_get(a_var var) {
    volatile a_var var2 = var;
    return *var2;
}

// various atomic operations on intptr_t numbers
static inline a_val a_add(a_var var, a_val v) { return __sync_add_and_fetch(var, v); }
static inline a_val a_sub(a_var var, a_val v) { return __sync_sub_and_fetch(var, v); }
static inline a_val a_or(a_var var, a_val v) { return __sync_or_and_fetch(var, v); }
static inline a_val a_and(a_var var, a_val v) { return __sync_and_and_fetch(var, v); }
static inline a_val a_xor(a_var var, a_val v) { return __sync_xor_and_fetch(var, v); }
static inline a_val a_nand(a_var var, a_val v) { return __sync_nand_and_fetch(var, v); }

// set the value of a atomic variable
// returns the new value of the variable
static inline a_val a_set(a_var var, a_val v) {
    volatile a_var var2 = var;
    return *var2 = v;
}

// set the value of a atomic variable only if the current value equals old
// returns the new value of the variable or the current value
static inline a_val a_set_if(a_var var, a_val v, a_val old) {
    a_val res = __sync_val_compare_and_swap(var, old, v);
    if (res == old) return v;
    return res;
}

// sets the value of a atomic variable only if the current value equals old
// retuns the value of the variable before the swap
static inline a_val a_swap_if(a_var var, a_val v, a_val old) {
    return __sync_val_compare_and_swap(var, old, v);
}

// sets the value of a atomic variable, but returns the old value
static inline a_val a_swap(a_var var, a_val v) {
    // sometimes I just cry ...
    a_val cur = a_get(var);
    a_val old;
    for(;;) {
        old = a_swap_if(var, v, cur);
        if (old == cur) return old;
    }
}

