// author: Onne Gorter, license: MIT (see license.txt)

// This is the hotel programming language public API.

#ifndef _tl_h_
#define _tl_h_
#pragma once

#include "platform.h"

// all values are tagged pointers, memory based values are aligned to 8 bytes, so 3 tag bits
typedef void* tlHandle;

typedef tlHandle tlSym;
typedef tlHandle tlInt;

// a kind describes a value, sort of like a common vtable
typedef struct tlKind tlKind;

// all memory based values start with a descriptive kind pointer
// notice also tlKind's are 8 byte aligned, so we have a pointer and 3 flags
typedef struct tlHead {
    intptr_t kind;
} tlHead;

// small int, 31 or 63 bits, lowest bit is always 1
static const tlHandle tlZero = (tlHead*)((0 << 1)|1);
static const tlHandle tlOne =  (tlHead*)((1 << 1)|1);
static const tlHandle tlTwo =  (tlHead*)((2 << 1)|1);
#define TL_MAX_INT32 ((int32_t)0x3FFFFFFF)
#define TL_MIN_INT32 ((int32_t)0xBFFFFFFF)
#ifdef M32
static const tlHandle tlIntMax = (tlHead*)(0x7FFFFFFF);
static const tlHandle tlIntMin = (tlHead*)(0xFFFFFFFF);
#define TL_MAX_INT ((int32_t)0x3FFFFFFF)
#define TL_MIN_INT ((int32_t)0xBFFFFFFF)
#else
static const tlHandle tlIntMax = (tlHead*)(0x7FFFFFFFFFFFFFFF);
static const tlHandle tlIntMin = (tlHead*)(0xFFFFFFFFFFFFFFFF);
#define TL_MAX_INT ((int64_t)0x3FFFFFFFFFFFFFFF)
#define TL_MIN_INT ((int64_t)0xBFFFFFFFFFFFFFFF)
#endif

// symbols are tlString* tagged with 010 and address > 1024
// so we use values tagged with 010 and < 1024 to encode some special values
#define TL_UNDEFINED ((1 << 3)|2)
#define TL_NULL      ((2 << 3)|2)
#define TL_FALSE     ((3 << 3)|2)
#define TL_TRUE      ((4 << 3)|2)
static const tlHandle tlUndefined = (tlHead*)TL_UNDEFINED;
static const tlHandle tlNull =      (tlHead*)TL_NULL;
static const tlHandle tlFalse =     (tlHead*)TL_FALSE;
static const tlHandle tlTrue =      (tlHead*)TL_TRUE;

static const tlHandle tlTaskNotRunning = (tlHead*)((10 << 3)|2);
static const tlHandle tlTaskJumping    = (tlHead*)((11 << 3)|2);

static const tlHandle tlThunkNull =    (tlHead*)((50 << 3)|2);
static const tlHandle tlCollectLazy =  (tlHead*)((51 << 3)|2);
static const tlHandle tlCollectEager = (tlHead*)((52 << 3)|2);

// for the attentive reader, where does 100 tag come into play?
// internally this is called an "active" value, which is used in the interpreter

// TODO remove ... a few defines
#define TL_MAX_PRIV_SIZE 7
#define TL_MAX_DATA_SIZE (65535 - TL_MAX_PRIV_SIZE)
#define TL_MAX_ARGS_SIZE 2000

static inline bool tlRefIs(tlHandle v) { return v && ((intptr_t)v & 7) == 0; }
static inline bool tlTagIs(tlHandle v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v < 1024; }

static inline bool tlBoolIs(tlHandle v) { return v == tlTrue || v == tlFalse; }
static inline tlHandle tlBoolAs(tlHandle v) { assert(tlBoolIs(v)); return v; }
static inline tlHandle tlBoolCast(tlHandle v) { return tlBoolIs(v)?tlBoolAs(v):0; }

static inline bool tlIntIs(tlHandle v) { return ((intptr_t)v & 1) == 1; }
static inline tlInt tlIntAs(tlHandle v) { assert(tlIntIs(v)); return v; }
static inline tlInt tlIntCast(tlHandle v) { return tlIntIs(v)?tlIntAs(v):0; }

static inline bool tlSymIs(tlHandle v) { return ((intptr_t)v & 7) == 2 && (intptr_t)v >= 1024; }
static inline tlSym tlSymAs(tlHandle v) { assert(tlSymIs(v)); return (tlSym)v; }
static inline tlSym tlSymCast(tlHandle v) { return tlSymIs(v)?tlSymAs(v):0; }

static inline tlHead* tl_head(tlHandle v) { assert(tlRefIs(v)); return (tlHead*)v; }

extern tlKind* tlIntKind;
extern tlKind* tlSymKind;
extern tlKind* tlNullKind;
extern tlKind* tlUndefinedKind;
extern tlKind* tlBoolKind;

static inline intptr_t get_kptr(tlHandle v) { return ((tlHead*)v)->kind; }

static inline tlKind* tl_kind(tlHandle v) {
    if (tlRefIs(v)) return (tlKind*)(get_kptr(v) & ~0x7);
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
static inline bool _T##Is(tlHandle v) { \
    return tl_kind(v) == _T##Kind; } \
static inline _T* _T##As(tlHandle v) { \
    assert(_T##Is(v) || !v); return (_T*)v; } \
static inline _T* _T##Cast(tlHandle v) { \
    return _T##Is(v)?(_T*)v:null; }

TL_REF_TYPE(tlFloat);
TL_REF_TYPE(tlString);
TL_REF_TYPE(tlSet);
TL_REF_TYPE(tlList);
TL_REF_TYPE(tlMap);
TL_REF_TYPE(tlHandleObject);

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


bool tlCallableIs(tlHandle v);

// to and from primitive values
tlHandle tlBOOL(unsigned c);
tlInt tlINT(intptr_t i);
tlFloat* tlFLOAT(double d);

// tlString and tlSYM can only be used after tl_init()
// TODO remove tlSYM because that requires a *global* *leaking* table ...
#define tlSTR(x) tlStringFromStatic(x, strlen(x))
#define tlSYM(x) tlSymFromStatic(x, strlen(x))

int tl_bool(tlHandle v);
int tl_bool_or(tlHandle v, bool d);
intptr_t tl_int(tlHandle v);
intptr_t tl_int_or(tlHandle v, int d);
double tl_double(tlHandle v);
double tl_double_or(tlHandle v, double d);
const char* tl_str(tlHandle v);

// ** main api for values in runtime **

// Any vararg (...) arguments must end with a null.
// Most getters don't require the current task.
// Mutable setters (ending with underscore) must only be used after you just created the value.
// Notice hotel level text, list, etc. values are not necesairy primitive tlStrings or tlLists etc.

unsigned int tlHandleHash(tlHandle v);
bool tlHandleEquals(tlHandle left, tlHandle right);
int tlHandleCompare(tlHandle left, tlHandle right);


// ** text **
int tlStringSize(tlString* str);
const char* tlStringData(tlString* str);

tlString* tlStringEmpty();

tlString* tlStringFromStatic(const char* s, int len);
tlString* tlStringFromCopy(const char* s, int len);
tlString* tlStringFromTake(char* s, int len);

tlString* tlStringCat(tlString* lhs, tlString* rhs);
tlString* tlStringSub(tlString* from, int first, int size);


// ** symbols **
tlSym tlSymFromStatic(const char* s, int len);
tlSym tlSymFromCopy(const char* s, int len);
tlSym tlSymFromTake(char* s, int len);

tlSym tlSymFromString(tlString* str);
tlString* tlStringFromSym(tlSym s);
const char* tlSymData(tlSym sym);


// ** list **
int tlListSize(tlList* list);
int tlListIsEmpty(tlList* list);
tlHandle tlListGet(tlList* list, int at);

tlList* tlListEmpty();
tlList* tlListNew(int size);
tlList* tlListFrom1(tlHandle v);
tlList* tlListFrom2(tlHandle v1, tlHandle v2);
tlList* tlListFrom(tlHandle v1, ... /*tlHandle*/);

tlList* tlListCopy(tlList* from, int newsize);
tlList* tlListAppend(tlList* list, tlHandle v);
tlList* tlListAppend2(tlList* list, tlHandle v1, tlHandle v2);
tlList* tlListPrepend(tlList* list, tlHandle v);
tlList* tlListPrepend2(tlList* list, tlHandle v1, tlHandle v2);
tlList* tlListSet(tlList* list, int at, tlHandle v);
tlList* tlListCat(tlList* lhs, tlList* rhs);
tlList* tlListSub(tlList* list, int first, int size);
tlList* tlListSplice(tlList* list, int first, int size);

void tlListSet_(tlList* list, int at, tlHandle v);


// ** map **
int tlMapSize(tlMap* map);
int tlMapIsEmpty(tlMap* map);
tlHandle tlMapGetInt(tlMap* map, int key);
tlHandle tlMapGetSym(tlMap* map, tlSym key);
tlHandle tlMapValueIter(tlMap* map, int i);
tlHandle tlMapKeyIter(tlMap* map, int i);

void tlMapValueIterSet_(tlMap* map, int i, tlHandle v);
void tlMapSetInt_(tlMap* map, int key, tlHandle v);
void tlMapSetSym_(tlMap* map, tlSym key, tlHandle v);
tlMap* tlMapToObject_(tlMap* map);

tlSet* tlSetNew(int size);
int tlSetAdd_(tlSet* set, tlHandle key);

tlMap* tlMapEmpty();
tlMap* tlMapNew(tlSet* keys);
tlMap* tlMapCopy(tlMap* map);
tlMap* tlMapToObject(tlMap* map);
tlMap* tlMapFrom1(tlHandle key, tlHandle v);
tlMap* tlMapFrom(tlHandle v1, ... /*tlHandle, tlHandle*/);
tlMap* tlMapFromMany(tlHandle vs[], int len /*multiple of 2*/);
tlMap* tlMapFromList(tlList* ls);
tlMap* tlMapFromPairs(tlList* ls);

tlHandle tlMapGet(tlMap* map, tlHandle key);
tlMap* tlMapSet(tlMap* map, tlHandle key, tlHandle v);
tlMap* tlMapDel(tlMap* map, tlHandle key);
tlMap* tlMapJoin(tlMap* lhs, tlMap* rhs);


// ** creating and running vms **
void tl_init();

tlVm* tlVmNew();
tlVm* tlVmCurrent();
void tlVmInitDefaultEnv(tlVm* vm);
void tlVmGlobalSet(tlVm* vm, tlSym key, tlHandle v);
void tlVmDelete(tlVm* vm);
int tlVmExitCode(tlVm* vm);

// register an external "task" as still running
void tlVmIncExternal(tlVm* vm);
void tlVmDecExternal(tlVm* vm);


// ** running code **

// get the current task
//tlTask* tlTaskCurrent();


// parsing
tlHandle tlParse(tlString* str, tlString* file);

// runs until all tasks are done
tlTask* tlVmEval(tlVm* vm, tlHandle v);
tlTask* tlVmEvalCall(tlVm* vm, tlHandle fn, ...);
tlTask* tlVmEvalBoot(tlVm* vm, tlArgs* as);
tlTask* tlVmEvalCode(tlVm* vm, tlString* code, tlString* file, tlArgs* as);
tlTask* tlVmEvalFile(tlVm* vm, tlString* file, tlArgs* as);

// setup a single task to eval something, returns immedately
tlHandle tlEval(tlHandle v);
tlHandle tlEvalCall(tlHandle fn, ...);
tlTask* tlEvalCode(tlVm* vm, tlString* code, tlArgs* as);

// reading status from tasks
bool tlTaskIsDone(tlTask* task);
tlHandle tlTaskGetThrowValue(tlTask* task);
tlHandle tlTaskGetValue(tlTask* task);

void tlTaskEnqueue(tlTask* task, lqueue* q);
tlTask* tlTaskDequeue(lqueue* q);

// run a task inside the vm, when done, unblock the current thread
// do not call from insideo a vm thread
tlTask* tlBlockingTaskNew(tlVm* vm);
tlHandle tlBlockingTaskEval(tlTask* task, tlHandle call);

// invoking toString, almost same as tlEvalCode("v.toString")
tlString* tltoString(tlHandle v);


// ** extending hotel with native functions **

// native functions signature
typedef tlHandle(*tlNativeCb)(tlArgs*);
tlNative* tlNATIVE(tlNativeCb cb, const char* name);
tlNative* tlNativeNew(tlNativeCb cb, tlSym name);

// bulk create native functions and globals (don't overuse these)
typedef struct { const char* name; tlNativeCb cb; } tlNativeCbs;
void tl_register_natives(const tlNativeCbs* cbs);
void tl_register_global(const char* name, tlHandle v);

// task management
tlTask* tlTaskNew(tlVm* vm, tlMap* locals);
tlVm* tlTaskGetVm(tlTask* task);

// stack reification
// native frame activation/resume signature
// if res is set, a call returned a regular result
// otherwise, if throw is set, a call has thrown a value (likely a tlError)
// if both are null, we are "unwinding" the stack for a jump (return/continuation ...)
typedef tlHandle(*tlResumeCb)(tlFrame* frame, tlHandle res, tlHandle throw);

// pause task to reify stack
tlHandle tlTaskPause(void* frame);
tlHandle tlTaskPauseAttach(void* frame);
tlHandle tlTaskPauseResuming(tlResumeCb resume, tlHandle res);

// mark task as waiting
tlHandle tlTaskWaitFor(tlHandle on);

// mark task as ready, will add to run queue, might be picked up immediately
void tlTaskReady();

// mark the current task as waiting for external event
tlTask* tlTaskWaitExternal();
void tlTaskReadyExternal(tlTask* task);
void tlTaskSetValue(tlTask* task, tlHandle h);

// throws value, if it is a map with a stack, it will be filled in witha stacktrace
tlHandle tlTaskThrow(tlHandle err);

// throw errors
tlHandle tlErrorThrow(tlHandle msg);
tlHandle tlArgumentErrorThrow(tlHandle msg);

#define TL_THROW(f, x...) do {\
    char _s[2048]; int _k = snprintf(_s, sizeof(_s), f, ##x);\
    return tlErrorThrow(tlStringFromCopy(_s, _k)); } while (0)
#define TL_ILLARG(f, x...) do {\
    char _s[2048]; int _k = snprintf(_s, sizeof(_s), f, ##x);\
    return tlArgumentErrorThrow(tlStringFromCopy(_s, _k)); } while (0)
#define TL_THROW_SET(f, x...) do {\
    char _s[2048]; snprintf(_s, sizeof(_s), f, ##x);\
    tlTaskThrowTake(strdup(_s)); } while (0)

// args
tlArgs* tlArgsNew(tlList* list, tlMap* map);
void tlArgsSet_(tlArgs* args, int at, tlHandle v);
void tlArgsMapSet_(tlArgs* args, tlSym name, tlHandle v);

tlHandle tlArgsTarget(tlArgs* args);
tlSym tlArgsMsg(tlArgs* args);

tlHandle tlArgsFn(tlArgs* args);
int tlArgsSize(tlArgs* args);
tlHandle tlArgsGet(tlArgs* args, int at);
int tlArgsMapSize(tlArgs* args);
tlHandle tlArgsMapGet(tlArgs* args, tlSym name);

// calls

tlCall* tlCallFrom(tlHandle h, ...);

// results, from a result or a value, get the first result or the value itself
tlHandle tlFirst(tlHandle v);
tlHandle tlResultGet(tlHandle v, int at);
// returning multiple results from native functions
tlResult* tlResultFrom(tlHandle v1, ...);

// allocate values
tlHandle tlAlloc(tlKind* kind, size_t bytes);
tlHandle tlClone(tlHandle v);

// TODO this should be done different
tlFrame* tlAllocFrame(tlResumeCb resume, size_t bytes);

// create objects with only native functions (to use as classes)
tlMap* tlClassMapFrom(const char* n1, tlNativeCb fn1, ...);


// ** environment (scope) **
tlEnv* tlEnvNew(tlEnv* parent);
tlHandle tlEnvGet(tlEnv* env, tlSym key);
tlEnv* tlEnvSet(tlEnv* env, tlSym key, tlHandle v);

tlEnv* tlVmGlobalEnv(tlVm* vm);

tlWorker* tlWorkerCurrent();

tlWorker* tlWorkerNew(tlVm* vm);
void tlWorkerDelete(tlWorker* worker);

// when creating your own worker, run this ... (not implemented yet)
void tlWorkerRun(tlWorker* worker);

// todo too low level?
tlHandle tlEvalArgsFn(tlArgs* args, tlHandle fn);
tlHandle tlEvalArgsTarget(tlArgs* args, tlHandle target, tlHandle fn);

typedef struct tlLock tlLock;

bool tlLockIs(tlHandle v);
tlLock* tlLockAs(tlHandle v);
tlLock* tlLockCast(tlHandle v);
tlTask* tlLockOwner(tlLock* lock);

typedef const char*(*tltoStringFn)(tlHandle v, char* buf, int size);
typedef unsigned int(*tlHashFn)(tlHandle from);
typedef int(*tlEqualsFn)(tlHandle left, tlHandle right);
typedef int(*tlCompareFn)(tlHandle left, tlHandle right);
typedef size_t(*tlByteSizeFn)(tlHandle from);
typedef tlHandle(*tlCallFn)(tlCall* args);
typedef tlHandle(*tlRunFn)(tlHandle fn, tlArgs* args);
typedef tlHandle(*tlSendFn)(tlArgs* args);

// a kind describes a hotel value for the vm (not at the language level)
// a kind can have a tlMap* klass, which represents its class from the language level
// fields with a null value is usually perfectly fine
struct tlKind {
    const char* name;  // general name of the value
    tltoStringFn toString; // a toString "printer" function

    tlHashFn hash;     // return a hash value, must be stable
    tlEqualsFn equals; // return left == right, required if hash is not null
    tlCompareFn cmp;   // return left - right

    tlByteSizeFn size; // return memory use in bytes, e.g. `return sizeof(tlTask)`

    tlCallFn call;     // if called as a function, before arguments are evaluated
    tlRunFn run;       // if called as a function, after arguments are evaluated

    bool locked;       // if the object has a lock, the task will aqcuire the lock
    tlSendFn send;     // if send a message args.this and args.msg are filled in
    tlMap* klass;      // if send a message, lookup args.msg here and eval its field
};

// any non-value object will have to be protected from concurrent access
// NOTE: if we keep all tasks in a global list, we can use 32 (16) bits for the task, and 32 bits as queue
//       there was a paper about that; collapses 3 pointers into one and has better behavior
struct tlLock {
    intptr_t kind;
    tlTask* owner;
    lqueue wait_q;
};

// any hotel "operation" can be paused and resumed
// Any state that needs to be saved needs to be captured in a tlFrame
// all frames together form a call stack
struct tlFrame {
    intptr_t kind;
    tlFrame* caller;     // the frame below/after us
    tlResumeCb resumecb; // the resume function to be called when this frame is to resume
};

// force similar layout
union _tl_0 { tlHead _tl_1; tlLock _tl_2; tlFrame _tl_3; };


// ** buffer **

TL_REF_TYPE(tlBuffer);
tlBuffer* tlBufferNew();
tlBuffer* tlBufferFromFile(const char* file);

int tlBufferSize(tlBuffer* buf);
const char* tlBufferData(tlBuffer* buf);

int tlBufferFind(tlBuffer* buf, const char* text, int len);
int tlBufferRead(tlBuffer* buf, char* to, int count);


#endif // _tl_h_

