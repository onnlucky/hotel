// author: Onne Gorter <onne@onnlucky.com>
#ifndef _tl_h_
#define _tl_h_
#pragma once

#include <stdint.h>
#include <assert.h>
#include "platform.h"

typedef void* tlValue;
typedef tlValue tlSym;
typedef tlValue tlInt;

// common head of all ref values; values appear in memory at 8 byte alignments
// thus pointers to values have 3 lowest bits set to 000
typedef struct tlHead {
    uint8_t flags;
    uint8_t type;
    uint16_t size;
    int32_t keep;
} tlHead;

// this is how all values look in memory
typedef struct tlData {
    tlHead head;
    tlValue data[];
} tlData;


// various flags we use; cannot mix freely <GC(2)|USER|USER | PRIV|FREE|PRIVCOUNT(2)>

// if HASFREE is set, privfield[1] is free function
static const uint8_t TL_FLAG_HASFREE  = 0x08;
static const uint8_t TL_FLAG_HASPRIV  = 0x04;
static inline int tl_privcount(tlValue v) { return 1 + (((tlHead*)v)->flags & 0x03); }

// used for tlCall and tlArgs and tlMsg objects to indicate named params were passed in
static const uint8_t TL_FLAG_HASKEYS  = 0x10;

// used for tlMap to indicate it must act as object and not a map
static const uint8_t TL_FLAG_ISOBJECT = 0x10;

// used for tlEnv to indicate it was captured or closed
static const uint8_t TL_FLAG_CAPTURED = 0x10;
static const uint8_t TL_FLAG_CLOSED   = 0x20;

// used by GC
static const uint8_t TL_FLAG_GCMARK1    = 0x40;
static const uint8_t TL_FLAG_GCMARK2    = 0x80;

// usable by user on own values
static const uint8_t TL_FLAG_1          = 0x10;
static const uint8_t TL_FLAG_2          = 0x20;


// small int, 31 or 63 bits, lowest bit is always 1
static const tlValue tlZero = (tlHead*)((0 << 1)|1);
static const tlValue tlOne =  (tlHead*)((1 << 1)|1);
static const tlValue tlTwo =  (tlHead*)((2 << 1)|1);
#ifdef M32
static const tlValue tlIntMax = (tlHead*)(0x7FFFFFFF);
static const tlValue tlIntMin = (tlHead*)(0xFFFFFFFF);
#define TL_MAX_INT ((int32_t)0x3FFFFFFF)
#define TL_MIN_INT ((int32_t)0xBFFFFFFF)
#else
static const tlValue tlIntMax = (tlHead*)(0x7FFFFFFFFFFFFFFF);
static const tlValue tlIntMin = (tlHead*)(0xFFFFFFFFFFFFFFFF);
#define TL_MAX_INT ((int64_t)0x3FFFFFFFFFFFFFFF)
#define TL_MIN_INT ((int64_t)0xBFFFFFFFFFFFFFFF)
#endif

// symbols are tlText* tagged with 010 and address > 1024
// so we use values tagged with 010 and < 1024 to encode some special values
static const tlValue tlUndefined = (tlHead*)((1 << 3)|2);
static const tlValue tlNull =      (tlHead*)((2 << 3)|2);
static const tlValue tlFalse =     (tlHead*)((3 << 3)|2);
static const tlValue tlTrue =      (tlHead*)((4 << 3)|2);

static const tlValue tlThunkNull =    (tlHead*)((50 << 3)|2);
static const tlValue tlCollectLazy =  (tlHead*)((51 << 3)|2);
static const tlValue tlCollectEager = (tlHead*)((52 << 3)|2);

// a few defines
#define TL_MAX_PRIV_SIZE 7
#define TL_MAX_DATA_SIZE (65535 - TL_MAX_PRIV_SIZE)
#define TL_MAX_ARGS_SIZE 2000

// all known primitive types
enum {
    TLInvalid = 0,

    // these never appear in head->type since these are tagged
    TLUndefined, TLNull, TLBool, TLSym, TLInt,

    TL_TYPE_TAGGED,

    TLNum,
    TLFloat,
    TLText,

    TLList, TLSet, TLMap,
    TLCall, TLArgs, TLMsg,

    TLEnv,
    TLClosure,
    TLThunk,
    TLLazy,

    TLCode,
    TLLookup,
    TLCollect,

    TLResult,
    TLError,

    TLRun,
    TLTask,
    TLWorker,
    TLVm,

    // extension values and functions
    TLTagged,
    TLHostFn,

    TL_TYPE_LAST
};
typedef uint8_t tlType;

static inline bool tlref_is(tlValue v) { return ((intptr_t)v & 7) == 0; }
static inline tlHead* tl_head(tlValue v) { assert(tlref_is(v)); return (tlHead*)v; }

static inline bool tldata_is(tlValue v) { return tlref_is(v); }
static inline tlData* tldata_as(tlValue v) { assert(tldata_is(v) || !v); return (tlData*)v; }
static inline tlData* tldata_cast(tlValue v) { return tldata_is(v)?(tlData*)v:null; }

#define TL_TYPE(SMALL, CAPS) \
typedef struct tl##CAPS tl##CAPS; \
static inline bool tl##SMALL##_is(tlValue v) { \
    return tlref_is(v) && tl_head(v)->type == TL##CAPS; } \
static inline tl##CAPS* tl##SMALL##_as(tlValue v) { \
    assert(tl##SMALL##_is(v) || !v); return (tl##CAPS*)v; } \
static inline tl##CAPS* tl##SMALL##_cast(tlValue v) { \
    return tl##SMALL##_is(v)?(tl##CAPS*)v:null; }

TL_TYPE(num, Num);
TL_TYPE(float, Float);
TL_TYPE(text, Text);

TL_TYPE(list, List);
TL_TYPE(set, Set);
TL_TYPE(map, Map);

TL_TYPE(call, Call);
TL_TYPE(args, Args);
TL_TYPE(msg, Msg);

TL_TYPE(env, Env);
TL_TYPE(closure, Closure);
TL_TYPE(thunk, Thunk);
TL_TYPE(lazy, Lazy);

TL_TYPE(code, Code);
TL_TYPE(lookup, Lookup);
TL_TYPE(collect, Collect);

TL_TYPE(result, Result);
TL_TYPE(error, Error);

TL_TYPE(run, Run);
TL_TYPE(task, Task);
TL_TYPE(worker, Worker);
TL_TYPE(vm, Vm);

TL_TYPE(tagged, Tagged);
TL_TYPE(hostfn, HostFn);

#undef TL_TYPE

static inline bool tlspecial_is(tlValue v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v < 1024; }

static inline bool tlint_is(tlValue v) { return ((intptr_t)v & 1) == 1; }
static inline tlInt tlint_as(tlValue v) { assert(tlint_is(v)); return v; }
static inline tlInt tlint_cast(tlValue v) { return tlint_is(v)?tlint_as(v):0; }

static inline bool tlsym_is(tlValue v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v >= 1024; }
static inline tlSym tlsym_as(tlValue v) { assert(tlsym_is(v)); return (tlSym)v; }
static inline tlSym tlsym_cast(tlValue v) { return tlsym_is(v)?tlsym_as(v):0; }

#if 1
bool tlactive_is(tlValue v);
tlValue tlvalue_from_active(tlValue a);
#else
static inline bool tlactive_is(tlValue v) {
    return ((intptr_t)v & 7) == 4 || ((intptr_t)v & 7) == 6;
}
static inline tlValue tlvalue_from_active(tlValue v) {
    assert(tlactive_is(v)); return (tlValue)((intptr_t)v & ~4);
}
#endif

// simple primitive functions
tlValue tlBOOL(unsigned c);
tlInt tlINT(int i);
tlValue tlACTIVE(tlValue v);

// tlTEXT and tlSYM can only be used after tlvm_new()
#define tlTEXT tltext_from_static
#define tlSYM tlsym_from_static
tlText* tltext_from_static(const char* s);
tlSym tlsym_from_static(const char* s);

int tl_bool(tlValue v);
int tl_bool_or(tlValue v, bool d);
int tl_int(tlValue v);
int tl_int_or(tlValue v, int d);
const char* tl_str(tlValue v);

// ** main api for values in runtime **

// Any vararg (...) arguments must end with a null.
// Most getters don't require the current task.
// Mutable setters (ending with underscore) must only be used after you just created the value.
// Notice hotel level text, list, etc. values are not necesairy primitive tlTexts or tlLists etc.

// ** text **
int tltext_size(tlText* text);
const char* tltext_data(tlText* text);

tlText* tltext_empty();
tlText* tltext_from_copy(tlTask* task, const char* s);
tlText* tltext_from_take(tlTask* task, char* s);
tlText* tlvalue_to_text(tlTask* task, tlValue v);

tlSym tlsym_from_copy(tlTask* task, const char* s);
tlSym tlsym_from_take(tlTask* task, char* s);
tlSym tlsym_from_text(tlTask* task, tlText* text);
tlText* tltext_from_sym(tlSym s);

tlText* tltext_cat(tlTask* task, tlText* lhs, tlText* rhs);
tlText* tltext_sub(tlTask* task, tlText* from, int first, int size);

// ** list **
int tllist_size(tlList* list);
int tllist_isempty(tlList* list);
tlValue tllist_get(tlList* list, int at);

tlList* tllist_empty();
tlList* tllist_new(tlTask* task, int size);
tlList* tllist_copy(tlTask* task, tlList* from, int newsize);
tlList* tllist_from1(tlTask* task, tlValue v);
tlList* tllist_from2(tlTask* task, tlValue v1, tlValue v2);
tlList* tllist_from(tlTask* task, ... /*tlValue*/);
tlList* tllist_from_many(tlTask* task, tlValue vs[], int len);

tlList* tllist_append(tlTask* task, tlList* list, tlValue v);
tlList* tllist_append2(tlTask* task, tlList* list, tlValue v1, tlValue v2);
tlList* tllist_prepend(tlTask* task, tlList* list, tlValue v);
tlList* tllist_prepend2(tlTask* task, tlList* list, tlValue v1, tlValue v2);
tlList* tllist_set(tlTask* task, tlList* list, int at, tlValue v);
tlList* tllist_cat(tlTask*, tlList* lhs, tlList* rhs);
tlList* tllist_sub(tlTask* task, tlList* list, int first, int size);
tlList* tllist_splice(tlTask* task, tlList* list, int first, int size);

void tllist_set_(tlList* list, int at, tlValue v);

// ** map **
int tlmap_size(tlMap* map);
int tlmap_isempty(tlMap* map);
tlValue tlmap_get_int(tlMap* map, int key);
tlValue tlmap_get_sym(tlMap* map, tlSym key);
tlValue tlmap_value_iter(tlMap* map, int i);
tlValue tlmap_key_iter(tlMap* map, int i);
void tlmap_value_iter_set_(tlMap* map, int i, tlValue v);
void tlmap_set_int_(tlMap* map, int key, tlValue v);
void tlmap_set_sym_(tlMap* map, tlSym key, tlValue v);

tlMap* tlmap_new(tlTask* task, tlSet* keys);
tlMap* tlmap_copy(tlTask* task, tlMap* map);
tlMap* tlmap_from1(tlTask* task, tlValue key, tlValue v);
tlMap* tlmap_from(tlTask* task, ... /*tlValue, tlValue*/);
tlMap* tlmap_from_many(tlTask* task, tlValue vs[], int len /*multiple of 2*/);
tlMap* tlmap_from_list(tlTask* task, tlList* ls);
tlMap* tlmap_from_pairs(tlTask* task, tlList* ls);

tlValue tlmap_get(tlTask* task, tlMap* map, tlValue key);
tlMap* tlmap_set(tlTask* task, tlMap* map, tlValue key, tlValue v);
tlMap* tlmap_del(tlTask* task, tlMap* map, tlValue key);
tlMap* tlmap_join(tlTask* task, tlMap* lhs, tlMap* rhs);

// ** code **
tlCode* tlcode_from(tlTask* task, tlList* stms);

tlValue tlargs_fn(tlArgs* args);
int tlargs_size(tlArgs* args);
tlValue tlargs_get(tlArgs* args, int at);
int tlargs_named_size(tlArgs* args);
tlValue tlargs_named_get(tlArgs* args, tlSym name);

tlCall* tlcall_from(tlTask* task, ...);
tlCall* tlcall_from_list(tlTask* task, tlValue fn, tlList* args);
tlValue tlcall_fn(tlCall* call);
tlValue tlcall_arg(tlCall* call, int at);
tlValue tlcall_arg_name(tlCall* call, int at);
tlSet* tlcall_names(tlCall* call);
bool tlcall_name_exists(tlCall* call, tlSym name);

void tlcall_fn_set_(tlCall* call, tlValue v);
void tlcall_set_(tlCall* call, int at, tlValue v);

// ** environment (scope) **
tlEnv* tlenv_new(tlTask* task, tlEnv* parent);
tlValue tlenv_get(tlTask* task, tlEnv* env, tlSym key);
tlEnv* tlenv_set(tlTask* task, tlEnv* env, tlSym key, tlValue v);

// ** vm management **
tlVm* tlvm_new();
void tlvm_delete(tlVm* vm);

tlWorker* tlvm_create_worker(tlVm* vm);
void tlworker_delete(tlWorker* worker);
void tlworker_run(tlWorker* worker);

void tlworker_attach(tlWorker* worker, tlTask* task);
// these all detach from the task
void tlworker_task_detach(tlTask* task);
void tlworker_task_ready(tlTask* task);
void tlworker_task_waitfor(tlTask* task, tlTask* other);

// ** task management **
tlTask* tltask_new(tlWorker* worker); // starts attached
tlTask* tltask_set_background_(tlTask* task, bool bg);
tlWorker* tltask_worker(tlTask* task);
tlVm* tltask_vm(tlTask* task);

void tltask_call(tlTask* task, tlCall* args);
bool tltask_isdone(tlTask* task);
tlValue tltask_value(tlTask* task);
tlValue tltask_exception(tlTask* task);

tlRun* tltask_return(tlTask* task, tlValue v);
tlRun* tltask_return_many(tlTask* task, tlValue vs[], int len);
tlRun* tltask_throw(tlTask* task, tlValue v);
tlRun* tltask_throw_str(tlTask* task, const char* msg);
#define TL_RETURN(v) return tltask_return(task, v)
#define TL_THROW(str) return tltask_throw_str(task, str)
#define TL_THROW_VALUE(v) return tltask_throw(task, v)

// ** callbacks **
typedef void(*tlFreeCb)(tlValue);
typedef void(*tlWorkCb)(tlTask*);
typedef tlRun*(*tlResumeCb)(tlTask*, tlRun*);
typedef tlRun*(*tlHostCb)(tlTask*, tlArgs*);

// ** memory **
tlValue tltask_alloc(tlTask* task, tlType type, int bytes, int fields);
tlValue tltask_alloc_privs(tlTask* task, tlType type, int bytes, int fields,
                           int privs, tlFreeCb freecb);
tlValue tltask_clone(tlTask* task, tlValue v);
#define TL_ALLOC(_T_, _FC_) tltask_alloc(task, TL##_T_, sizeof(tl##_T_), _FC_)
#define TL_CLONE(_V_) tltask_clone(task, _V_)

// ** extending **

tlHostFn* tlhostfn_new(tlTask* task, tlHostCb cb, int fields);
void tlhostfn_set_(tlHostFn* fun, int at, tlValue v);

typedef struct { const char* name; tlHostCb cb; } tlHostCbs;
void tl_register_global(const char* name, tlValue v);
void tl_register_hostcbs(const tlHostCbs* cbs);

// ** running **
tlValue tl_parse(tlTask* task, tlText* text);

#endif // _tl_h_
