// author: Onne Gorter <onne@onnlucky.com>
#ifndef _tl_h_
#define _tl_h_
#pragma once

#include <stdint.h>
#include <assert.h>
#include "platform.h"

typedef void* tValue;
typedef tValue tSym;
typedef tValue tLookup;
typedef tValue tInt;

// common head of all ref values
typedef struct tHead {
    uint8_t flags;
    uint8_t type;
    uint16_t size;
#ifdef M64
    uint32_t pad;
#endif
} tHead;

// this is how all values look in memory
typedef struct tList {
    tHead head;
    tValue data[];
} tList;

#define T_PRIVATE_SIZE(s) (sizeof(s) / sizeof(tValue) - 1)
static inline int _private(tValue v) { return ((tList*)v)->head.flags & 0x0F; }

// various flags we use; cannot mix randomly
static const uint8_t T_FLAG_NOFREE = 0x10;
static const uint8_t T_FLAG_HASKEYS = 0x10;
static const uint8_t T_FLAG_HASLIST = 0x20;
static const uint8_t T_FLAG_INCALL = 0x10;
static const uint8_t T_FLAG_INARGS = 0x20;

// static predefined value
static const tValue tUndefined = (tHead*)(1 << 2);
static const tValue tNull =      (tHead*)(2 << 2);
static const tValue tFalse =     (tHead*)(3 << 2);
static const tValue tTrue =      (tHead*)(4 << 2);
static const tValue tZero = (tHead*)((0 << 1)|1);
static const tValue tOne =  (tHead*)((1 << 1)|1);
static const tValue tTwo =  (tHead*)((2 << 1)|1);
#ifdef M32
static const tValue tIntMax = (tHead*)(0x7FFFFFFF);
static const tValue tIntMin = (tHead*)(0xFFFFFFFF);
#define T_MAX_INT ((int32_t)0x3FFFFFFF)
#define T_MIN_INT ((int32_t)0xBFFFFFFF)
#else
static const tValue tIntMax = (tHead*)(0x7FFFFFFFFFFFFFFF);
static const tValue tIntMin = (tHead*)(0xFFFFFFFFFFFFFFFF);
#define T_MAX_INT ((int64_t)0x3FFFFFFFFFFFFFFF)
#define T_MIN_INT ((int64_t)0xBFFFFFFFFFFFFFFF)
#endif

static const tValue tThunkNull =    (tHead*)(50 << 2);
static const tValue tCollectLazy =  (tHead*)(51 << 2);
static const tValue tCollectEager = (tHead*)(52 << 2);

#define T_MAX_DATA_SIZE 65530
#define T_MAX_ARGS_SIZE 2000

// all known primitive types
enum {
    TInvalid = 0,

    TList, TMap, TObject,

    TNum, TFloat,
    TText,

    TEnv,

    TBody,
    TClosure,
    TFun,

    TCall,

    TEvalCall,
    TEvalFun,
    TEval,

    TThunk,
    TResult,
    TError,

    TVar,
    TTask,

    TLAST,

    // these never appear in head->type since these are tagged
    TUndefined, TNull, TBool, TSym, TInt,
};

// a list is also a map, but a map may be a sparse list or map other type of values to keys
typedef struct tList tMap;
typedef struct tText tText;

static inline int tref_is(tValue v) { return ((intptr_t)v & 3) == 0 && (intptr_t)v > 1024; }
static inline tHead* t_head(tValue v) { assert(tref_is(v)); return (tHead*)v; }

static inline int tint_is(tValue v) { return ((intptr_t)v & 1); }
static inline tInt tint_as(tValue v) { assert(tint_is(v)); return v; }
static inline tInt tint_cast(tValue v) { return tint_is(v)?tint_as(v):0; }

static inline int tsym_is(tValue v) { return ((intptr_t)v & 2) && (intptr_t)v > 1024; }
static inline tSym tsym_as(tValue v) { assert(tsym_is(v)); return (tSym)v; }
static inline tSym tsym_cast(tValue v) { return tsym_is(v)?tsym_as(v):0; }

bool tactive_is(tValue v);
tValue tvalue_from_active(tValue v);

static inline int tlist_is(tValue v) { return tref_is(v) && t_head(v)->type == TList; }
static inline tList* tlist_as(tValue v) { assert(tlist_is(v)); return (tList*)v; }
static inline tList* tlist_cast(tValue v) { return tlist_is(v)?tlist_as(v):0; }

static inline int tmap_is(tValue v) { return tref_is(v) && t_head(v)->type <= TObject; }
static inline tMap* tmap_as(tValue v) { assert(tmap_is(v)); return (tMap*)v; }
static inline tMap* tmap_cast(tValue v) { return tmap_is(v)?tmap_as(v):0; }

static inline int ttext_is(tValue v) { return tref_is(v) && t_head(v)->type == TText; }
static inline tText* ttext_as(tValue v) { assert(ttext_is(v)); return (tText*)v; }
static inline tText* ttext_cast(tValue v) { return ttext_is(v)?ttext_as(v):0; }

// simple primitive functions
tValue tBOOL(unsigned c);
tInt tINT(int i);

// tTEXT and tSYM are only to be used in before tvm_init();
#define tTEXT ttext_from_static
tText* ttext_from_static(const char* s);
#define tSYM tsym_from_static
tSym tsym_from_static(const char* s);

tValue tACTIVE(tValue v);

int t_bool(tValue v);
int t_int(tValue v);
const char* t_str(tValue v);


// main api
typedef struct tVm tVm;
typedef struct tWorker tWorker;
typedef struct tTask tTask;
typedef struct tCall tCall;
typedef struct tSend tSend;
typedef struct tBody tBody;
#define tT tTask *task


// ** text **
// notice not all language text objects are actually tTexts, some are implemented using ropes
int ttext_size(tText* text);
const char* ttext_bytes(tText* text);

tText* ttext_empty();
tText* tvalue_to_text(tT, tValue v);

tText* ttext_from_copy(tT, const char* s);
tText* ttext_from_take(tT, char* s);

tSym tsym_from_copy(tT, const char* s);
tSym tsym_from(tT, tText* text);

tText* tsym_to_text(tSym s);

bool tbody_is(tValue v);
tBody* tbody_cast(tValue v);
tBody* tbody_as(tValue v);

tBody* tbody_from(tT, tList* stms);

bool tcall_is(tValue v);
tCall* tcall_cast(tValue v);
tCall* tcall_as(tValue v);

tCall* tcall_from_args(tT, tValue fn, tList* args);
tValue tcall_get_fn(tCall* call);
void tcall_set_fn_(tCall* call, tValue v);

bool tsend_is(tValue v);
tSend* tsend_cast(tValue v);
tSend* tsend_as(tValue v);

tValue tsend_get_oop(tSend* call);
void tsend_set_oop_(tSend* call, tValue v);


// ** list **
int tlist_size(tList* list);
int tlist_is_empty(tList* list);
tValue tlist_at(tList* list);

tList* tlist_empty();
tList* tlist_new(tT, int size);
tList* tlist_copy(tT, tList* list, int size);
tList* tlist_from1(tT, tValue v);
tList* tlist_from2(tT, tValue v1, tValue v2);
tList* tlist_from(tT, ... /*tValue*/);
tList* tlist_from_a(tT, tValue* vs, int len);
#define tLIST tlist_from1
#define tLIST2 tlist_from2

tList* tlist_add(tT, tList* list, tValue v);
tList* tlist_prepend(tT, tList* list, tValue v);
tList* tlist_set(tT, tList* list, int at, tValue v);
tList* tlist_cat(tT, tList* lhs, tList* rhs);
tList* tlist_slice(tT, tList* list, int first, int last);

void tlist_set_(tList* list, int at, tValue v);


// ** map **
int tmap_size(tMap* map);
int tmap_is_empty(tMap* map);
tValue tmap_get_int(tMap* map, int key);
tValue tmap_get_sym(tMap* map, tSym key);

tMap* tmap_new(tT, int size);
tMap* tmap_copy(tT, tMap* map);
tMap* tmap_from1(tT, tValue key, tValue v);
tMap* tmap_from(tT, ... /*tValue, tValue*/);
tMap* tmap_from_a(tT, tValue* vs, int len /*multiple of 2*/);
tMap* tmap_from_list(tT, tList* ls);
#define tMAP tmap_from1

tMap* tmap_get(tT, tMap* map, tValue key, tValue v);
tMap* tmap_cat(tT, tMap* lhs, tMap* rhs);

tMap* tmap_set(tT, tMap* map, tValue key, tValue v);
tMap* tmap_set_int(tT, tMap* map, int key, tValue v);
tMap* tmap_set_sym(tT, tMap* map, tSym key, tValue v);

// maps have to contain the key already
void tmap_set_int_(tMap* map, int key, tValue v);
void tmap_set_sym_(tMap* map, tSym key, tValue v);


// ** environment (scope) **
typedef struct tEnv tEnv;
tEnv* tenv_new(tT, tEnv* parent, tList* keys);
void tenv_close(tT, tEnv* env);
tValue tenv_get(tT, tEnv* env, tSym key);
tEnv* tenv_set(tT, tEnv* env, tSym key, tValue v);


// ** vm **
tVm* tvm_new();
void tvm_delete(tVm* vm);

tWorker* tvm_create_worker(tVm* vm);
void tworker_delete(tWorker* worker);
void tworker_run(tWorker* worker);

void tworker_attach(tWorker* worker, tTask* task);
void tworker_detach(tWorker* worker, tTask* task);


// ** tasks **
tTask* tvm_create_task(tVm* task);
void ttask_delete(tT);

tVm* ttask_get_vm(tT);
tWorker* ttask_get_worker(tT);

tValue ttask_value(tT);
tValue ttask_exception(tT);

void ttask_ready(tT);
void ttask_call(tT, tCall* args);

typedef int tRES;
tRES ttask_return1(tT, tValue);
tRES ttask_return(tT, ... /*tValue*/);
tRES ttask_return_a(tT, tValue* vs, int len);


// ** callbacks **
typedef tRES(*t_native)(tTask*, tMap*);
typedef struct tFun tFun;

// for general functions
tFun* tFUN(tSym name, t_native);
// for primitive functions that never invoke the evaluator again
tFun* tFUN_PRIM(tSym name, t_native);

tValue parse(tText* text);
tValue compile(tText* text);

#endif // _tl_h_

