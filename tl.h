// author: Onne Gorter, license: MIT (see license.txt)

// This is the hotel programming language public API.

#ifndef _tl_h_
#define _tl_h_
#pragma once

#include "platform.h"

typedef void* tlValue;
typedef tlValue tlSym;
typedef tlValue tlInt;

typedef struct tlClass tlClass;

// TODO how well defined is the struct memory layout we embed pretty deep sometimes
// TODO rework now that we have classes ... but into what?
// TODO we actually have 3 bits in the class pointer ... just incase
// common head of all ref values; values appear in memory at 8 byte alignments
// thus pointers to values have 3 lowest bits set to 000
typedef struct tlHead {
    uint8_t flags;
    uint8_t type;
    uint16_t size;
    int32_t keep;
    tlClass* klass;
} tlHead;

// TODO remove ...
// this is how all values look in memory
typedef struct tlData {
    tlHead head;
    tlValue data[];
} tlData;


// TODO remove and simplify ... any value can handle its own privately
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
#define TL_MAX_INT32 ((int32_t)0x3FFFFFFF)
#define TL_MIN_INT32 ((int32_t)0xBFFFFFFF)
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
#define TL_UNDEFINED ((1 << 3)|2)
#define TL_NULL      ((2 << 3)|2)
#define TL_FALSE     ((3 << 3)|2)
#define TL_TRUE      ((4 << 3)|2)
static const tlValue tlUndefined = (tlHead*)TL_UNDEFINED;
static const tlValue tlNull =      (tlHead*)TL_NULL;
static const tlValue tlFalse =     (tlHead*)TL_FALSE;
static const tlValue tlTrue =      (tlHead*)TL_TRUE;

static const tlValue tlTaskNotRunning = (tlHead*)((10 << 3)|2);
static const tlValue tlTaskJumping    = (tlHead*)((11 << 3)|2);

static const tlValue tlThunkNull =    (tlHead*)((50 << 3)|2);
static const tlValue tlCollectLazy =  (tlHead*)((51 << 3)|2);
static const tlValue tlCollectEager = (tlHead*)((52 << 3)|2);

// for the attentive reader, where does 100 tag come into play?
// internally this is called an "active" value, which is used in the interpreter

// TODO remove ... a few defines
#define TL_MAX_PRIV_SIZE 7
#define TL_MAX_DATA_SIZE (65535 - TL_MAX_PRIV_SIZE)
#define TL_MAX_ARGS_SIZE 2000

static inline bool tlRefIs(tlValue v) { return v && ((intptr_t)v & 7) == 0; }
static inline bool tlTagIs(tlValue v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v < 1024; }

static inline bool tlIntIs(tlValue v) { return ((intptr_t)v & 1) == 1; }
static inline tlInt tlIntAs(tlValue v) { assert(tlIntIs(v)); return v; }
static inline tlInt tlIntCast(tlValue v) { return tlIntIs(v)?tlIntAs(v):0; }

static inline bool tlSymIs(tlValue v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v >= 1024; }
static inline tlSym tlSymAs(tlValue v) { assert(tlSymIs(v)); return (tlSym)v; }
static inline tlSym tlSymCast(tlValue v) { return tlSymIs(v)?tlSymAs(v):0; }

static inline tlHead* tl_head(tlValue v) { assert(tlRefIs(v)); return (tlHead*)v; }

#define TL_TYPE(SMALL, CAPS) \
typedef struct tl##CAPS tl##CAPS; \
static inline bool tl##SMALL##_is(tlValue v) { \
    return tlRefIs(v) && tl_head(v)->type == TL##CAPS; } \
static inline tl##CAPS* tl##SMALL##_as(tlValue v) { \
    assert(tl##SMALL##_is(v) || !v); return (tl##CAPS*)v; } \
static inline tl##CAPS* tl##SMALL##_cast(tlValue v) { \
    return tl##SMALL##_is(v)?(tl##CAPS*)v:null; } \

extern tlClass* tlIntClass;
extern tlClass* tlSymClass;
extern tlClass* tlNullClass;
extern tlClass* tlUndefinedClass;
extern tlClass* tlBoolClass;

static inline tlClass* tl_class(tlValue v) {
    if (tlRefIs(v)) return tl_head(v)->klass;
    if (tlIntIs(v)) return tlIntClass;
    if (tlSymIs(v)) return tlSymClass;
    // assert(tlTagIs(v));
    switch ((intptr_t)v) {
        case TL_UNDEFINED: return tlUndefinedClass;
        case TL_NULL: return tlNullClass;
        case TL_FALSE: return tlBoolClass;
        case TL_TRUE: return tlBoolClass;
        default: return null;
    }
}

#define TL_REF_TYPE(_T) \
typedef struct _T _T; \
extern tlClass* _T##Class; \
static inline bool _T##Is(tlValue v) { \
    return tl_class(v) == _T##Class; } \
static inline _T* _T##As(tlValue v) { \
    assert(_T##Is(v) || !v); return (_T*)v; } \
static inline _T* _T##Cast(tlValue v) { \
    return _T##Is(v)?(_T*)v:null; }

TL_REF_TYPE(tlText);
TL_REF_TYPE(tlSet);
TL_REF_TYPE(tlList);
TL_REF_TYPE(tlMap);
TL_REF_TYPE(tlValueObject);

TL_REF_TYPE(tlCall);
TL_REF_TYPE(tlArgs);
TL_REF_TYPE(tlResult);
TL_REF_TYPE(tlError);

TL_REF_TYPE(tlEnv);
TL_REF_TYPE(tlFrame);

TL_REF_TYPE(tlNative);
TL_REF_TYPE(tlCode);

// TODO these 6 don't need to be exposed
TL_REF_TYPE(tlVar);
TL_REF_TYPE(tlObject);

TL_REF_TYPE(tlClosure);
TL_REF_TYPE(tlThunk);
TL_REF_TYPE(tlLazy);
TL_REF_TYPE(tlCollect);

TL_REF_TYPE(tlVm);
TL_REF_TYPE(tlWorker);
TL_REF_TYPE(tlTask);

bool tlCallableIs(tlValue v);

// TODO rework this a bit more ...
typedef tlValue(*tlSendFn)(tlTask* task, tlArgs* args);
typedef tlValue(*tlCallFn)(tlTask* task, tlCall* args);
typedef const char*(*tlToTextFn)(tlValue v, char* buf, int size);

struct tlClass {
    const char* name;
    tlToTextFn toText;

    tlMap* map;
    tlSendFn send;
    tlCallFn call;
};

// to and from primitive values
tlValue tlBOOL(unsigned c);
tlInt tlINT(int i);

// tlTEXT and tlSYM can only be used after tl_init()
// TODO remove tlSYM because that requires a *global* *leaking* table ...
#define tlTEXT(x) tlTextFromStatic(x, strlen(x))
#define tlSYM(x) tlSymFromStatic(x, strlen(x))

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
int tlTextSize(tlText* text);
const char* tlTextData(tlText* text);

tlText* tlTextEmpty();

tlText* tlTextFromStatic(const char* s, int len);
tlText* tlTextFromCopy(tlTask* task, const char* s, int len);
tlText* tlTextFromTake(tlTask* task, char* s, int len);

tlText* tlTextCat(tlTask* task, tlText* lhs, tlText* rhs);
tlText* tlTextSub(tlTask* task, tlText* from, int first, int size);


// ** symbols **
tlSym tlSymFromStatic(const char* s, int len);
tlSym tlSymFromCopy(tlTask* task, const char* s, int len);
tlSym tlSymFromTake(tlTask* task, char* s, int len);

tlSym tlSymFromText(tlTask* task, tlText* text);
tlText* tlTextFromSym(tlSym s);
const char* tlSymData(tlSym sym);


// ** list **
int tlListSize(tlList* list);
int tlListIsEmpty(tlList* list);
tlValue tlListGet(tlList* list, int at);

tlList* tlListEmpty();
tlList* tlListNew(tlTask* task, int size);
tlList* tlListFrom1(tlTask* task, tlValue v);
tlList* tlListFrom2(tlTask* task, tlValue v1, tlValue v2);
tlList* tlListFrom(tlTask* task, ... /*tlValue*/);

tlList* tlListCopy(tlTask* task, tlList* from, int newsize);
tlList* tlListAppend(tlTask* task, tlList* list, tlValue v);
tlList* tlListAppend2(tlTask* task, tlList* list, tlValue v1, tlValue v2);
tlList* tlListPrepend(tlTask* task, tlList* list, tlValue v);
tlList* tlListPrepend2(tlTask* task, tlList* list, tlValue v1, tlValue v2);
tlList* tlListSet(tlTask* task, tlList* list, int at, tlValue v);
tlList* tlListCat(tlTask*, tlList* lhs, tlList* rhs);
tlList* tlListSub(tlTask* task, tlList* list, int first, int size);
tlList* tlListSplice(tlTask* task, tlList* list, int first, int size);

void tlListSet_(tlList* list, int at, tlValue v);


// ** map **
int tlMapSize(tlMap* map);
int tlMapIsEmpty(tlMap* map);
tlValue tlMapGetInt(tlMap* map, int key);
tlValue tlMapGetSym(tlMap* map, tlSym key);
tlValue tlMapValueIter(tlMap* map, int i);
tlValue tlMapKeyIter(tlMap* map, int i);

void tlMapValueIterSet_(tlMap* map, int i, tlValue v);
void tlMapSetInt_(tlMap* map, int key, tlValue v);
void tlMapSetSym_(tlMap* map, tlSym key, tlValue v);
tlMap* tlMapToObject_(tlMap* map);

tlMap* tlMapEmpty();
tlMap* tlMapNew(tlTask* task, tlSet* keys);
tlMap* tlMapCopy(tlTask* task, tlMap* map);
tlMap* tlMapFrom1(tlTask* task, tlValue key, tlValue v);
tlMap* tlMapFrom(tlTask* task, ... /*tlValue, tlValue*/);
tlMap* tlMapFromMany(tlTask* task, tlValue vs[], int len /*multiple of 2*/);
tlMap* tlMapFromList(tlTask* task, tlList* ls);
tlMap* tlMapFromPairs(tlTask* task, tlList* ls);

tlValue tlMapGet(tlTask* task, tlMap* map, tlValue key);
tlMap* tlMapSet(tlTask* task, tlMap* map, tlValue key, tlValue v);
tlMap* tlMapDel(tlTask* task, tlMap* map, tlValue key);
tlMap* tlMapJoin(tlTask* task, tlMap* lhs, tlMap* rhs);


// ** creating and running vms **
void tl_init();

tlVm* tlVmNew();
void tlVmInitDefaultEnv(tlVm* vm);
void tlVmGlobalSet(tlVm* vm, tlSym key, tlValue v);
void tlVmDelete(tlVm* vm);


// ** running code **
// parsing
tlValue tlParse(tlTask* task, tlText* text);

// runs until all tasks are done
tlTask* tlVmEval(tlVm* vm, tlValue v);
tlTask* tlVmEvalCall(tlVm* vm, tlValue fn, ...);
tlTask* tlVmEvalCode(tlVm* vm, tlText* code);
tlTask* tlVmEvalFile(tlVm* vm, tlText* file);

// setup a single task to eval something, returns immedately
tlValue tlEval(tlTask* task, tlValue v);
tlValue tlEvalCall(tlTask* task, tlValue fn, ...);
tlTask* tlEvalCode(tlVm* vm, tlText* code);

// reading status from tasks
bool tlTaskIsDone(tlTask* task);
tlError* tlTaskGetError(tlTask* task);
tlValue tlTaskGetValue(tlTask* task);

// reading errors
tlValue tlErrorValue(tlError* error);

// invoking toText, almost same as tlEvalCode("v.toText")
tlText* tlToText(tlTask* task, tlValue v);


// ** extending hotel with native functions **

// native functions signature
typedef tlValue(*tlNativeCb)(tlTask*, tlArgs*);
tlNative* tlNATIVE(tlNativeCb cb, const char* name);
tlNative* tlNativeNew(tlTask* task, tlNativeCb cb, tlSym name);

// bulk create native functions and globals (don't overuse these)
typedef struct { const char* name; tlNativeCb cb; } tlNativeCbs;
void tl_register_natives(const tlNativeCbs* cbs);
void tl_register_global(const char* name, tlValue v);

// task management
tlTask* tlTaskNew(tlVm* vm);
tlVm* tlTaskGetVm(tlTask* task);

// stack reification
// native frame activation/resume signature
typedef tlValue(*tlResumeCb)(tlTask*, tlFrame*, tlValue, tlError*);
// pause task to reify stack
tlValue tlTaskPause(tlTask* task, void* frame);
tlValue tlTaskPauseAttach(tlTask* task, void* frame);
tlValue tlTaskPauseResuming(tlTask* task, tlResumeCb resume, tlValue res);
// mark task as waiting
tlValue tlTaskWaitFor(tlTask* task, tlValue on);
void tlTaskWaitIo(tlTask* task);
// mark task as ready, will add to run queue, might be picked up immediately
void tlTaskReady(tlTask* task);

// throw errors
tlValue tlTaskThrowTake(tlTask* task, char* str);
#define TL_THROW(f, x...) do { char _s[2048]; snprintf(_s, sizeof(_s), f, ##x); return tlTaskThrowTake(task, strdup(_s)); } while (0)
#define TL_THROW_SET(f, x...) do { char _s[2048]; snprintf(_s, sizeof(_s), f, ##x); tlTaskThrowTake(task, strdup(_s)); } while (0)

// args
tlValue tlArgsTarget(tlArgs* args);
tlSym tlArgsMsg(tlArgs* args);

tlValue tlArgsFn(tlArgs* args);
int tlArgsSize(tlArgs* args);
tlValue tlArgsGet(tlArgs* args, int at);
int tlArgsMapSize(tlArgs* args);
tlValue tlArgsMapGet(tlArgs* args, tlSym name);

// results, from a result or a value, get the first result or the value itself
tlValue tlFirst(tlValue v);
tlValue tlResultGet(tlValue v, int at);
// returning multiple results from native functions
tlResult* tlResultNewFrom(tlTask* task, ...);

// allocate values
void* tlAlloc(tlTask* task, tlClass* klass, size_t bytes);
void* tlAllocWithFields(tlTask* task, tlClass* klass, size_t bytes, int fieldc);
void* tlAllocClone(tlTask* task, tlValue v, size_t bytes);
tlFrame* tlAllocFrame(tlTask* task, tlResumeCb resume, size_t bytes);

// create objects with only native functions (to use as classes)
tlMap* tlClassMapFrom(const char* n1, tlNativeCb fn1, ...);


// ** environment (scope) **
tlEnv* tlEnvNew(tlTask* task, tlEnv* parent);
tlValue tlEnvGet(tlTask* task, tlEnv* env, tlSym key);
tlEnv* tlEnvSet(tlTask* task, tlEnv* env, tlSym key, tlValue v);

tlEnv* tlVmGlobalEnv(tlVm* vm);

tlWorker* tlWorkerNew(tlVm* vm);
void tlWorkerDelete(tlWorker* worker);
tlWorker* tlTaskWorker(tlTask* task);

// when creating your own worker, run this ... (not implemented yet)
void tlWorkerRun(tlWorker* worker);

tlValue tlEvalArgsFn(tlTask* task, tlArgs* args, tlValue fn);

typedef struct tlSynchronized {
    tlHead head;
    tlTask* owner;
    lqueue msg_q;
} tlSynchronized;

tlValue tlSynchronizedReceive(tlTask* task, tlArgs* args);
bool tlSynchronizedIs(tlValue v);
tlSynchronized* tlSynchronizedAs(tlValue v);
tlTask* tlSynchronizedOwner(tlSynchronized* sync);


#endif // _tl_h_

