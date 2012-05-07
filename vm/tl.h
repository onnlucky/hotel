// author: Onne Gorter, license: MIT (see license.txt)

// This is the hotel programming language public API.

#ifndef _tl_h_
#define _tl_h_
#pragma once

#include "platform.h"

typedef void* tlValue;
typedef tlValue tlSym;
typedef tlValue tlInt;

typedef struct tlKind tlKind;

// TODO how well defined is the struct memory layout we embed pretty deep sometimes
// TODO rework now that we have classes ... but into what?
// TODO we actually have 3 bits in the kind pointer ... just incase
// common head of all ref values; values appear in memory at 8 byte alignments
// thus pointers to values have 3 lowest bits set to 000
typedef struct tlHead {
    uint8_t xxxflags;
    uint16_t size;
    tlKind* kind;
} tlHead;

// used for tlCall and tlArgs and tlMsg objects to indicate named params were passed in
static const uint8_t TL_FLAG_HASKEYS  = 0x10;

// used for tlEnv to indicate it was captured or closed
static const uint8_t TL_FLAG_CAPTURED = 0x10;
static const uint8_t TL_FLAG_CLOSED   = 0x20;

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

static inline bool tlBoolIs(tlValue v) { return v == tlTrue || v == tlFalse; }
static inline tlValue tlBoolAs(tlValue v) { assert(tlBoolIs(v)); return v; }
static inline tlValue tlBoolCast(tlValue v) { return tlBoolIs(v)?tlBoolAs(v):0; }

static inline bool tlIntIs(tlValue v) { return ((intptr_t)v & 1) == 1; }
static inline tlInt tlIntAs(tlValue v) { assert(tlIntIs(v)); return v; }
static inline tlInt tlIntCast(tlValue v) { return tlIntIs(v)?tlIntAs(v):0; }

static inline bool tlSymIs(tlValue v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v >= 1024; }
static inline tlSym tlSymAs(tlValue v) { assert(tlSymIs(v)); return (tlSym)v; }
static inline tlSym tlSymCast(tlValue v) { return tlSymIs(v)?tlSymAs(v):0; }

static inline tlHead* tl_head(tlValue v) { assert(tlRefIs(v)); return (tlHead*)v; }

extern tlKind* tlIntKind;
extern tlKind* tlSymKind;
extern tlKind* tlNullKind;
extern tlKind* tlUndefinedKind;
extern tlKind* tlBoolKind;

static inline tlKind* tl_kind(tlValue v) {
    if (tlRefIs(v)) return tl_head(v)->kind;
    if (tlIntIs(v)) return tlIntKind;
    if (tlSymIs(v)) return tlSymKind;
    // assert(tlTagIs(v));
    switch ((intptr_t)v) {
        case TL_UNDEFINED: return tlUndefinedKind;
        case TL_NULL: return tlNullKind;
        case TL_FALSE: return tlBoolKind;
        case TL_TRUE: return tlBoolKind;
        default: return null;
    }
}

#define TL_REF_TYPE(_T) \
typedef struct _T _T; \
extern tlKind* _T##Kind; \
static inline bool _T##Is(tlValue v) { \
    return tl_kind(v) == _T##Kind; } \
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
tlText* tlTextFromCopy(const char* s, int len);
tlText* tlTextFromTake(char* s, int len);

tlText* tlTextCat(tlText* lhs, tlText* rhs);
tlText* tlTextSub(tlText* from, int first, int size);


// ** symbols **
tlSym tlSymFromStatic(const char* s, int len);
tlSym tlSymFromCopy(const char* s, int len);
tlSym tlSymFromTake(char* s, int len);

tlSym tlSymFromText(tlText* text);
tlText* tlTextFromSym(tlSym s);
const char* tlSymData(tlSym sym);


// ** list **
int tlListSize(tlList* list);
int tlListIsEmpty(tlList* list);
tlValue tlListGet(tlList* list, int at);

tlList* tlListEmpty();
tlList* tlListNew(int size);
tlList* tlListFrom1(tlValue v);
tlList* tlListFrom2(tlValue v1, tlValue v2);
tlList* tlListFrom(tlValue v1, ... /*tlValue*/);

tlList* tlListCopy(tlList* from, int newsize);
tlList* tlListAppend(tlList* list, tlValue v);
tlList* tlListAppend2(tlList* list, tlValue v1, tlValue v2);
tlList* tlListPrepend(tlList* list, tlValue v);
tlList* tlListPrepend2(tlList* list, tlValue v1, tlValue v2);
tlList* tlListSet(tlList* list, int at, tlValue v);
tlList* tlListCat(tlList* lhs, tlList* rhs);
tlList* tlListSub(tlList* list, int first, int size);
tlList* tlListSplice(tlList* list, int first, int size);

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
tlMap* tlMapNew(tlSet* keys);
tlMap* tlMapCopy(tlMap* map);
tlMap* tlMapFrom1(tlValue key, tlValue v);
tlMap* tlMapFrom(tlValue v1, ... /*tlValue, tlValue*/);
tlMap* tlMapFromMany(tlValue vs[], int len /*multiple of 2*/);
tlMap* tlMapFromList(tlList* ls);
tlMap* tlMapFromPairs(tlList* ls);

tlValue tlMapGet(tlMap* map, tlValue key);
tlMap* tlMapSet(tlMap* map, tlValue key, tlValue v);
tlMap* tlMapDel(tlMap* map, tlValue key);
tlMap* tlMapJoin(tlMap* lhs, tlMap* rhs);


// ** creating and running vms **
void tl_init();

tlVm* tlVmNew();
void tlVmInitDefaultEnv(tlVm* vm);
void tlVmGlobalSet(tlVm* vm, tlSym key, tlValue v);
void tlVmDelete(tlVm* vm);


// ** running code **

// get the current task
//tlTask* tlTaskCurrent();

// parsing
tlValue tlParse(tlText* text, tlText* file);

// runs until all tasks are done
tlTask* tlVmEval(tlVm* vm, tlValue v);
tlTask* tlVmEvalCall(tlVm* vm, tlValue fn, ...);
tlTask* tlVmEvalBoot(tlVm* vm, tlArgs* as);
tlTask* tlVmEvalCode(tlVm* vm, tlText* code, tlText* file, tlArgs* as);
tlTask* tlVmEvalFile(tlVm* vm, tlText* file, tlArgs* as);

// setup a single task to eval something, returns immedately
tlValue tlEval(tlValue v);
tlValue tlEvalCall(tlValue fn, ...);
tlTask* tlEvalCode(tlVm* vm, tlText* code, tlArgs* as);

// reading status from tasks
bool tlTaskIsDone(tlTask* task);
tlValue tlTaskGetThrowValue(tlTask* task);
tlValue tlTaskGetValue(tlTask* task);

// invoking toText, almost same as tlEvalCode("v.toText")
tlText* tlToText(tlValue v);


// ** extending hotel with native functions **

// native functions signature
typedef tlValue(*tlNativeCb)(tlArgs*);
tlNative* tlNATIVE(tlNativeCb cb, const char* name);
tlNative* tlNativeNew(tlNativeCb cb, tlSym name);

// bulk create native functions and globals (don't overuse these)
typedef struct { const char* name; tlNativeCb cb; } tlNativeCbs;
void tl_register_natives(const tlNativeCbs* cbs);
void tl_register_global(const char* name, tlValue v);

// task management
tlTask* tlTaskNew(tlVm* vm, tlMap* locals);
tlVm* tlTaskGetVm(tlTask* task);

// stack reification
// native frame activation/resume signature
// if res is set, a call returned a regular result
// otherwise, if throw is set, a call has thrown a value (likely a tlError)
// if both are null, we are "unwinding" the stack for a jump (return/continuation ...)
typedef tlValue(*tlResumeCb)(tlFrame* frame, tlValue res, tlValue throw);
// pause task to reify stack
tlValue tlTaskPause(void* frame);
tlValue tlTaskPauseAttach(void* frame);
tlValue tlTaskPauseResuming(tlResumeCb resume, tlValue res);
// mark task as waiting
tlValue tlTaskWaitFor(tlValue on);
void tlTaskWaitIo();
// mark task as ready, will add to run queue, might be picked up immediately
void tlTaskReady();

// throws value, if it is a map with a stack, it will be filled in witha stacktrace
tlValue tlTaskThrow(tlValue err);

// throw errors
tlValue tlTaskThrowTake(char* str);
#define TL_THROW(f, x...) do { char _s[2048]; snprintf(_s, sizeof(_s), f, ##x); return tlTaskThrowTake(strdup(_s)); } while (0)
#define TL_THROW_SET(f, x...) do { char _s[2048]; snprintf(_s, sizeof(_s), f, ##x); tlTaskThrowTake(strdup(_s)); } while (0)

// args
tlArgs* tlArgsNew(tlList* list, tlMap* map);
void tlArgsSet_(tlArgs* args, int at, tlValue v);
void tlArgsMapSet_(tlArgs* args, tlSym name, tlValue v);

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
tlResult* tlResultFrom(tlValue v1, ...);

// allocate values
void* tlAlloc(tlKind* kind, size_t bytes);
void* tlAllocWithFields(tlKind* kind, size_t bytes, int fieldc);
void* tlAllocClone(tlValue v, size_t bytes);
tlFrame* tlAllocFrame(tlResumeCb resume, size_t bytes);

// create objects with only native functions (to use as classes)
tlMap* tlClassMapFrom(const char* n1, tlNativeCb fn1, ...);


// ** environment (scope) **
tlEnv* tlEnvNew(tlEnv* parent);
tlValue tlEnvGet(tlEnv* env, tlSym key);
tlEnv* tlEnvSet(tlEnv* env, tlSym key, tlValue v);

tlEnv* tlVmGlobalEnv(tlVm* vm);

tlWorker* tlWorkerCurrent();

tlWorker* tlWorkerNew(tlVm* vm);
void tlWorkerDelete(tlWorker* worker);

// when creating your own worker, run this ... (not implemented yet)
void tlWorkerRun(tlWorker* worker);

// todo too low level?
tlValue tlEvalArgsFn(tlArgs* args, tlValue fn);
tlValue tlEvalArgsTarget(tlArgs* args, tlValue target, tlValue fn);

typedef struct tlLock tlLock;

bool tlLockIs(tlValue v);
tlLock* tlLockAs(tlValue v);
tlLock* tlLockCast(tlValue v);
tlTask* tlLockOwner(tlLock* lock);

typedef const char*(*tlToTextFn)(tlValue v, char* buf, int size);
typedef tlValue(*tlCallFn)(tlCall* args);
typedef tlValue(*tlRunFn)(tlValue fn, tlArgs* args);
typedef tlValue(*tlSendFn)(tlArgs* args);

// a kind describes a hotel value for the vm (not at the language level)
// fields with a null value is usually perfectly fine
struct tlKind {
    const char* name;  // general name of the value
    tlToTextFn toText; // a toText "printer" function

    tlCallFn call;     // if called as a function, before arguments are evaluated
    tlRunFn run;       // if called as a function, after arguments are evaluated

    tlSendFn send;     // if send a message args.this and args.msg are filled in
    tlMap* map;        // if send a message, lookup args.msg here and eval its field

    bool locked;       // if the object has a lock, the task will aqcuire the lock
};

// any non-value object will have to be protected from concurrent access
// NOTE: if we keep all tasks in a global list, we can use 32 (16) bits for the task, and 32 bits as queue
//       there was a paper about that; collapses 3 pointers into one and has better behavior
struct tlLock {
    tlHead head;
    tlTask* owner;
    lqueue wait_q;
};

// any hotel "operation" can be paused and resumed
// Any state that needs to be saved needs to be captured in a tlFrame
// all frames together form a call stack
struct tlFrame {
    tlHead head;
    tlFrame* caller;     // the frame below/after us
    tlResumeCb resumecb; // the resume function to be called when this frame is to resume
};

#endif // _tl_h_

