// author: Onne Gorter <onne@onnlucky.com>
#ifndef _tl_h_
#define _tl_h_
#pragma once

#include <stdint.h>
#include <assert.h>
#include "platform.h"

typedef void* tlValue;
typedef tlValue tlSym;
typedef tlValue tlLookup;
typedef tlValue tlInt;

// common head of all ref values
typedef struct tlHead {
    uint8_t flags;
    uint8_t type;
    uint16_t size;
    int32_t keep;
} tlHead;

// this is how all values look in memory
typedef struct tlList {
    tlHead head;
    tlValue data[];
} tlList;

#define TL_PRIVATE_SIZE(s) (sizeof(s) / sizeof(tlValue) - 1)
static inline int _private(tlValue v) { return ((tlList*)v)->head.flags & 0x0F; }

// various flags we use; cannot mix randomly
static const uint8_t TL_FLAG_NOFREE = 0x10;
static const uint8_t TL_FLAG_HASKEYS = 0x10;
static const uint8_t TL_FLAG_HASLIST = 0x20;
static const uint8_t TL_FLAG_ISOBJECT = 0x40;

static const uint8_t TL_FLAG_CLOSED = 0x40;

// static predefined value
static const tlValue tlUndefined = (tlHead*)(1 << 2);
static const tlValue tlNull =      (tlHead*)(2 << 2);
static const tlValue tlFalse =     (tlHead*)(3 << 2);
static const tlValue tlTrue =      (tlHead*)(4 << 2);
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

static const tlValue tlThunkNull =    (tlHead*)(50 << 2);
static const tlValue tlCollectLazy =  (tlHead*)(51 << 2);
static const tlValue tlCollectEager = (tlHead*)(52 << 2);

#define TL_MAX_DATA_SIZE 65530
#define TL_MAX_ARGS_SIZE 2000

// all known primitive types
enum {
    TLInvalid = 0,

    TLList, TLMap, TLObject,

    TLNum, TLFloat,
    TLText,

    TLEnv,

    TLCode,
    TLClosure,
    TLFun,

    TLCall,

    TLRun,

    TLEvalCall,
    TLEvalFun,
    TLEval,

    TLThunk,
    TLResult,
    TLCollect,
    TLError,

    TLVar,
    TLTask,

    TLLAST,

    // these never appear in head->type since these are tagged
    TLUndefined, TLNull, TLBool, TLSym, TLInt,

    TL_TYPE_LAST
};

// a list is also a map, but a map may be a sparse list or map other type of values to keys
typedef struct tlList tlMap;
typedef struct tlText tlText;

static inline int tlref_is(tlValue v) { return ((intptr_t)v & 3) == 0 && (intptr_t)v > 1024; }
static inline tlHead* tl_head(tlValue v) { assert(tlref_is(v)); return (tlHead*)v; }

static inline int tlint_is(tlValue v) { return ((intptr_t)v & 1); }
static inline tlInt tlint_as(tlValue v) { assert(tlint_is(v)); return v; }
static inline tlInt tlint_cast(tlValue v) { return tlint_is(v)?tlint_as(v):0; }

static inline int tlsym_is(tlValue v) { return ((intptr_t)v & 2) && (intptr_t)v > 1024; }
static inline tlSym tlsym_as(tlValue v) { assert(tlsym_is(v)); return (tlSym)v; }
static inline tlSym tlsym_cast(tlValue v) { return tlsym_is(v)?tlsym_as(v):0; }

bool tlactive_is(tlValue v);
tlValue tlvalue_from_active(tlValue v);

static inline int tllist_is(tlValue v) { return tlref_is(v) && tl_head(v)->type == TLList; }
static inline tlList* tllist_as(tlValue v) { assert(tllist_is(v)); return (tlList*)v; }
static inline tlList* tllist_cast(tlValue v) { return tllist_is(v)?tllist_as(v):0; }

static inline int tlmap_is(tlValue v) { return tlref_is(v) && tl_head(v)->type <= TLObject; }
static inline tlMap* tlmap_as(tlValue v) { assert(tlmap_is(v)); return (tlMap*)v; }
static inline tlMap* tlmap_cast(tlValue v) { return tlmap_is(v)?tlmap_as(v):0; }

static inline int tltext_is(tlValue v) { return tlref_is(v) && tl_head(v)->type == TLText; }
static inline tlText* tltext_as(tlValue v) { assert(tltext_is(v)); return (tlText*)v; }
static inline tlText* tltext_cast(tlValue v) { return tltext_is(v)?tltext_as(v):0; }

// simple primitive functions
tlValue tlBOOL(unsigned c);
tlInt tlINT(int i);

// tlTEXT and tlSYM are only to be used in before tlvm_init();
#define tlTEXT tltext_from_static
tlText* tltext_from_static(const char* s);
#define tlSYM tlsym_from_static
tlSym tlsym_from_static(const char* s);

tlValue tlACTIVE(tlValue v);

int tl_bool(tlValue v);
int tl_int(tlValue v);
int tl_int_or(tlValue v, int d);
const char* tl_str(tlValue v);


// main api
typedef struct tlVm tlVm;
typedef struct tlWorker tlWorker;
typedef struct tlTask tlTask;
typedef struct tlCall tlCall;
typedef struct tlCode tlCode;
#define tlT tlTask *task


// ** text **
// notice not all language text objects are actually tlTexts, some are implemented using ropes
int tltext_size(tlText* text);
const char* tltext_bytes(tlText* text);

tlText* tltext_empty();
tlText* tlvalue_to_text(tlT, tlValue v);

tlText* tltext_from_copy(tlT, const char* s);
tlText* tltext_from_take(tlT, char* s);

tlSym tlsym_from_copy(tlT, const char* s);
tlSym tlsym_from(tlT, tlText* text);

tlText* tlsym_to_text(tlSym s);

bool tlcode_is(tlValue v);
tlCode* tlcode_cast(tlValue v);
tlCode* tlcode_as(tlValue v);

tlCode* tlcode_from(tlT, tlList* stms);

bool tlcall_is(tlValue v);
tlCall* tlcall_cast(tlValue v);
tlCall* tlcall_as(tlValue v);

tlCall* tlcall_from(tlTask* task, ...);
tlCall* tlcall_from_args(tlT, tlValue fn, tlList* args);
tlValue tlcall_get_fn(tlCall* call);
void tlcall_set_fn_(tlCall* call, tlValue v);


// ** list **
int tllist_size(tlList* list);
int tllist_is_empty(tlList* list);
tlValue tllist_at(tlList* list);

tlList* tllist_empty();
tlList* tllist_new(tlT, int size);
tlList* tllist_copy(tlT, tlList* list, int size);
tlList* tllist_from1(tlT, tlValue v);
tlList* tllist_from2(tlT, tlValue v1, tlValue v2);
tlList* tllist_from(tlT, ... /*tlValue*/);
tlList* tllist_from_a(tlT, tlValue* vs, int len);
#define tlLIST tllist_from1
#define tlLIST2 tllist_from2

tlList* tllist_add(tlT, tlList* list, tlValue v);
tlList* tllist_prepend(tlT, tlList* list, tlValue v);
tlList* tllist_set(tlT, tlList* list, int at, tlValue v);
tlList* tllist_cat(tlT, tlList* lhs, tlList* rhs);
tlList* tllist_slice(tlT, tlList* list, int first, int last);

void tllist_set_(tlList* list, int at, tlValue v);


// ** map **
int tlmap_size(tlMap* map);
int tlmap_is_empty(tlMap* map);
tlValue tlmap_get_int(tlMap* map, int key);
tlValue tlmap_get_sym(tlMap* map, tlSym key);
tlValue tlmap_value_iter(tlMap* map, int i);

tlMap* tlmap_new(tlT, int size);
tlMap* tlmap_copy(tlT, tlMap* map);
tlMap* tlmap_from1(tlT, tlValue key, tlValue v);
tlMap* tlmap_from(tlT, ... /*tlValue, tlValue*/);
tlMap* tlmap_from_a(tlT, tlValue* vs, int len /*multiple of 2*/);
tlMap* tlmap_from_list(tlT, tlList* ls);
#define tlMAP tlmap_from1

tlValue tlmap_get(tlT, tlMap* map, tlValue key);
tlMap* tlmap_cat(tlT, tlMap* lhs, tlMap* rhs);

tlMap* tlmap_set(tlT, tlMap* map, tlValue key, tlValue v);
tlMap* tlmap_set_int(tlT, tlMap* map, int key, tlValue v);
tlMap* tlmap_set_sym(tlT, tlMap* map, tlSym key, tlValue v);

// maps have to contain the key already
void tlmap_set_int_(tlMap* map, int key, tlValue v);
void tlmap_set_sym_(tlMap* map, tlSym key, tlValue v);


// ** environment (scope) **
typedef struct tlEnv tlEnv;
tlEnv* tlenv_new(tlT, tlEnv* parent);
tlValue tlenv_get(tlT, tlEnv* env, tlSym key);
tlEnv* tlenv_set(tlT, tlEnv* env, tlSym key, tlValue v);


// ** vm **
tlVm* tlvm_new();
void tlvm_delete(tlVm* vm);

tlWorker* tlvm_create_worker(tlVm* vm);
void tlworker_delete(tlWorker* worker);
void tlworker_run(tlWorker* worker);

void tlworker_attach(tlWorker* worker, tlTask* task);
void tlworker_detach(tlWorker* worker, tlTask* task);


// ** tasks **
tlTask* tlvm_create_task(tlVm* task);
void tltask_delete(tlT);

tlVm* tltask_get_vm(tlT);
tlWorker* tltask_get_worker(tlT);

tlValue tltask_value(tlT);
tlValue tltask_exception(tlT);

void tltask_ready(tlT);
void tltask_call(tlT, tlCall* args);

typedef int tlRES;
tlRES tltask_return1(tlT, tlValue);
tlRES tltask_return_a(tlT, tlValue* vs, int len);


// ** callbacks **
typedef struct tlFun tlFun;
typedef tlValue(*tlHostFunction)(tlTask*, tlFun*, tlMap*);

typedef struct { const char* name; tlHostFunction fn; } tlHostFunctions;
static void tl_register_functions(const tlHostFunctions* fns);

void tl_register_const(const char* name, tlValue v);

// for general functions
tlFun* tlFUN(tlHostFunction, tlValue data);
// for primitive functions that never invoke the evaluator again
tlFun* tlFUN_PRIM(tlHostFunction, tlValue data);

tlValue parse(tlText* text);
tlValue compile(tlText* text);

#endif // _tl_h_

