// author: Onne Gorter, license: MIT (see license.txt)

// This is the hotel programming language public API.

#ifndef _tl_h_
#define _tl_h_

#include <stdint.h> // int64_t, etc...
#include <unistd.h> // size_t
#include <stdio.h>  // snprintf
#include <string.h> // strlen

#ifndef bool
#define bool int
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#ifndef null
#define null ((void*)0)
#endif

#ifndef assert
#define assert(e) ((void)0)
#endif

#ifndef _lqueue_h_
typedef struct lqentry lqentry;
typedef struct lqueue lqueue;
struct lqentry { lqentry* next; };
struct lqueue { lqentry* head; lqentry* tail; };
#endif

// all values are tagged pointers, memory based values are aligned to 8 bytes, so 3 tag bits
typedef void* tlHandle;

typedef tlHandle tlInt;
typedef tlHandle tlMini;
typedef tlHandle tlSym;

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
#define TL_MIN_INT32 ((int32_t)0xC0000000)
#ifdef M32
static const tlHandle tlIntMax = (tlHead*)((int32_t)0x7FFFFFFF);
static const tlHandle tlIntMin = (tlHead*)((int32_t)0x80000001);
#define TL_MAX_INT ((int32_t)0x3FFFFFFF)
#define TL_MIN_INT ((int32_t)0xC0000000)
#else
static const tlHandle tlIntMax = (tlHead*)((int64_t)0x7FFFFFFFFFFFFFFF);
static const tlHandle tlIntMin = (tlHead*)((int64_t)0x8000000000000001);
#define TL_MAX_INT ((int64_t)0x3FFFFFFFFFFFFFFF)
#define TL_MIN_INT ((int64_t)0xC000000000000000)
#endif

// mini strings are 7 (or 3) byte chars encode directly into the pointer, ending with 0b010
// symbols are tlString* tagged with 0b100 and address > 1024
// so we use values tagged with 0b100 and < 1024 to encode some special values
#define TL_NULL      ((2 << 3)|2)
#define TL_FALSE     ((3 << 3)|2)
#define TL_TRUE      ((4 << 3)|2)
static const tlHandle tlNull =      (tlHead*)TL_NULL;
static const tlHandle tlFalse =     (tlHead*)TL_FALSE;
static const tlHandle tlTrue =      (tlHead*)TL_TRUE;
static const tlHandle tlUnknown =   (tlHead*)((5 << 3)|4);

static const tlHandle tlTaskNotRunning = (tlHead*)((10 << 3)|4);
static const tlHandle tlTaskJumping    = (tlHead*)((11 << 3)|4);

static const tlHandle tlThunkNull =    (tlHead*)((50 << 3)|4);
static const tlHandle tlCollectLazy =  (tlHead*)((51 << 3)|4);
static const tlHandle tlCollectEager = (tlHead*)((52 << 3)|4);

static inline bool tlRefIs(tlHandle v) { return v && ((intptr_t)v & 7) == 0; }
static inline bool tlTagIs(tlHandle v) { return ((intptr_t)v & 7) == 4 && (intptr_t)v < 1024; }

static inline bool tlNullIs(tlHandle v) { return v == tlNull; }
static inline bool tlBoolIs(tlHandle v) { return v == tlTrue || v == tlFalse; }
static inline tlHandle tlBoolAs(tlHandle v) { assert(tlBoolIs(v)); return v; }
static inline tlHandle tlBoolCast(tlHandle v) { return tlBoolIs(v)?tlBoolAs(v):0; }

static inline bool tlIntIs(tlHandle v) { return ((intptr_t)v & 1) == 1; }
static inline tlInt tlIntAs(tlHandle v) { assert(tlIntIs(v)); return v; }
static inline tlInt tlIntCast(tlHandle v) { return tlIntIs(v)?tlIntAs(v):0; }

// TODO not used at the moment
static inline bool tlMiniIs_(tlHandle v) { return ((intptr_t)v & 7) == 2; }
static inline tlMini tlMiniAs_(tlHandle v) { assert(tlMiniIs_(v)); return v; }
static inline tlMini tlMiniCast_(tlHandle v) { return tlMiniIs_(v)?tlMiniAs_(v):0; }

static inline bool tlSymIs_(tlHandle v) { return ((intptr_t)v & 7) == 4 && (intptr_t)v >= 1024; }
static inline tlSym tlSymAs_(tlHandle v) { assert(tlSymIs_(v)); return (tlSym)v; }
static inline tlSym tlSymCast_(tlHandle v) { return tlSymIs_(v)?tlSymAs_(v):0; }

static inline tlHead* tl_head(tlHandle v) { assert(tlRefIs(v)); return (tlHead*)v; }

extern tlKind* tlIntKind;
extern tlKind* tlSymKind;
extern tlKind* tlNullKind;
extern tlKind* tlBoolKind;

static inline intptr_t get_kptr(tlHandle v) { return ((tlHead*)v)->kind; }

static inline tlKind* tl_kind(tlHandle v) {
    if (tlRefIs(v)) return (tlKind*)(get_kptr(v) & ~0x7);
    if (tlIntIs(v)) return tlIntKind;
    if (tlSymIs_(v)) return tlSymKind;
    switch ((intptr_t)v) {
        case TL_NULL: return tlNullKind;
        case TL_FALSE: return tlBoolKind;
        case TL_TRUE: return tlBoolKind;
        default: return null;
    }
}

#define TL_REF_TYPE(_T) \
typedef struct _T _T; \
extern tlKind* _T##Kind; \
static inline bool _T##Is(const tlHandle v) { \
    return tl_kind(v) == _T##Kind; } \
static inline _T* _T##As(tlHandle v) { \
    assert(_T##Is(v) || !v); return (_T*)v; } \
static inline _T* _T##Cast(tlHandle v) { \
    return _T##Is(v)?(_T*)v:null; }

#define INIT_KIND(n) n = malloc(sizeof(tlKind)); memcpy(n, &_##n, sizeof(tlKind));

typedef struct tlString tlString;
extern tlKind* tlStringKind;
static inline bool tlStringIs(const tlHandle v) {
    return tl_kind(v) == tlStringKind || tl_kind(v) == tlSymKind;
}
static inline tlString* tlStringAs(tlHandle v) {
    assert(tlStringIs(v));
    return (tlString*)((intptr_t)v & ~7);
}
static inline tlString* tlStringCast(tlHandle v) {
    if (!tlStringIs(v)) return null;
    return (tlString*)((intptr_t)v & ~7);
}

tlSym tlSymFromString(tlString* str);

static inline bool tlSymIs(tlHandle v) { return tlSymIs_(v) || tlStringIs(v); }
static inline tlSym tlSymAs(tlHandle v) { if (tlStringIs(v)) return tlSymFromString(v); return tlSymAs_(v); }
static inline tlSym tlSymCast(tlHandle v) { if (tlStringIs(v)) return tlSymFromString(v); return tlSymCast_(v); }

TL_REF_TYPE(tlBin);
TL_REF_TYPE(tlUndefined);
TL_REF_TYPE(tlFloat);
TL_REF_TYPE(tlNum);
TL_REF_TYPE(tlChar);
TL_REF_TYPE(tlSet);
TL_REF_TYPE(tlList);
TL_REF_TYPE(tlObject);
TL_REF_TYPE(tlClass);
TL_REF_TYPE(tlMap);

TL_REF_TYPE(tlArgs);
TL_REF_TYPE(tlResult);

TL_REF_TYPE(tlFrame);

TL_REF_TYPE(tlNative);
TL_REF_TYPE(tlCode);

TL_REF_TYPE(tlBuffer);
TL_REF_TYPE(tlHashMap);
TL_REF_TYPE(tlArray);

// TODO these 6 don't need to be exposed
TL_REF_TYPE(tlVar);
TL_REF_TYPE(tlMutable);

TL_REF_TYPE(tlClosure);
TL_REF_TYPE(tlThunk);
TL_REF_TYPE(tlLazy);
TL_REF_TYPE(tlCollect);

TL_REF_TYPE(tlVm);
TL_REF_TYPE(tlWorker);
TL_REF_TYPE(tlTask);
TL_REF_TYPE(tlDebugger);

TL_REF_TYPE(tlBModule);

TL_REF_TYPE(tlQueue);

bool tlCallableIs(tlHandle v);

// to and from primitive values
tlHandle tlUndef();
static inline tlHandle tlOR_UNDEF(tlHandle v) { return (v)? v : tlUndef(); }
static inline tlHandle tlOR_NULL(tlHandle v) { return (v)? v : tlNull; }
tlHandle tlBOOL(int c);
tlHandle tlINT(intptr_t i);
tlHandle tlFLOAT(double d);
tlHandle tlNUM(intptr_t n);
tlHandle tlCHAR(int c);
tlHandle tlPARSENUM(const char* s, int radix);
static inline bool tlNumberIs(tlHandle v) { return tlIntIs(v) || tlFloatIs(v) || tlNumIs(v); }
static inline tlHandle tlNumberCast(tlHandle v) { return tlNumberIs(v)? v : null; }

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
const char* tl_str(tlHandle v); // returns a const char* from an internal static pool, only to be used for debugging
char* tl_repr(tlHandle v);

int at_offset(tlHandle v, int size);

// ** main api for values in runtime **

// Any vararg (...) arguments must end with a null.
// Most getters don't require the current task.
// Mutable setters (ending with underscore) must only be used after you just created the value.
// Notice hotel level text, list, etc. values are not necesairy primitive tlStrings or tlLists etc.

extern tlHandle tlEqual;
extern tlHandle tlSmaller;
extern tlHandle tlLarger;
tlHandle tlCOMPARE(intptr_t); // returns one of tlEqual, tlSmaller, tlLarger

uint32_t tlHandleHash(tlHandle v);
bool tlHandleEquals(tlHandle left, tlHandle right);
tlHandle tlHandleCompare(tlHandle left, tlHandle right);

tlClass* tlCLASS(const char* name, tlNative* ctor, tlObject* methods, tlObject* statics);
#define tlMETHODS tlClassObjectFrom

// ** strings **
int tlStringSize(tlString* str);
int tlStringGet(tlString* str, int at);
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

tlString* tlStringFromSym(tlSym s);
const char* tlSymData(tlSym sym);


// ** list **
int tlListSize(const tlList* list);
int tlListIsEmpty(const tlList* list);
tlHandle tlListGet(const tlList* list, int at);

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


// ** object **
int tlObjectSize(const tlObject* o);
int tlObjectIsEmpty(const tlObject* o);
tlSet* tlObjectKeys(const tlObject* o);
tlHandle tlObjectGetInt(const tlObject* o, int key);
tlHandle tlObjectGetSym(const tlObject* o, tlSym key);
tlHandle tlObjectValueIter(const tlObject* o, int i);
tlHandle tlObjectKeyIter(const tlObject* o, int i);

void tlObjectValueIterSet_(tlObject* o, int i, tlHandle v);
void tlObjectSet_(tlObject* o, tlSym key, tlHandle v);
tlObject* tlMapToObject_(tlMap* o);

tlSet* tlSetEmpty();
tlSet* tlSetNew(int size);
tlHandle tlSetGet(tlSet* set, int at);
int tlSetAdd_(tlSet* set, tlHandle key);

tlObject* tlObjectEmpty();
tlObject* tlObjectNew(tlSet* keys);
tlObject* tlObjectCopy(tlObject* o);
// TODO tlObject is String -> Value and not just that, String.intern == true
tlObject* tlObjectFrom(const char* k1, tlHandle v1, ...);
tlObject* tlObjectFromMap(tlMap* map);
tlObject* tlObjectFromStrings(tlString* k1, tlHandle v1, ...);
tlObject* tlObjectFrom1(tlHandle key, tlHandle v);
tlObject* tlObjectFromMany(tlHandle vs[], int len /*multiple of 2*/);
tlObject* tlObjectFromList(tlList* ls);
tlObject* tlObjectFromPairs(tlList* ls);

tlHandle tlObjectGet(tlObject* o, tlHandle key);
tlObject* tlObjectSet(tlObject* o, tlHandle key, tlHandle v);
tlObject* tlObjectDel(tlObject* o, tlHandle key);
tlObject* tlObjectJoin(tlObject* lhs, tlObject* rhs);

tlMap* tlMapEmpty();
tlMap* tlMapNew(tlSet* keys);
tlMap* tlMapFromObject(tlObject* o);
tlMap* tlMapFromPairs(tlList* ls);
int tlMapSize(tlMap* map);
tlHandle tlMapGet(tlMap* map, tlHandle key);
tlHandle tlMapKeyIter(tlMap* map, int i);
tlHandle tlMapValueIter(tlMap* map, int i);
void tlMapValueIterSet_(tlMap* map, int i, tlHandle v);


// ** creating and running vms **
void tl_init();

tlVm* tlVmNew(tlSym argv0, tlArgs* args);
tlBuffer* tlVmGetInit();
tlBuffer* tlVmGetCompiler();
void tlVmInitDefaultEnv(tlVm* vm);
void tlVmDelete(tlVm* vm);
int tlVmExitCode(tlVm* vm);

tlHandle tlVmGlobalGet(tlVm* vm, tlSym key);
void tlVmGlobalSet(tlVm* vm, tlSym key, tlHandle v);
tlObject* tlVmGlobals(tlVm* vm);

// register an external "task" as still running
void tlVmIncExternal(tlVm* vm);
void tlVmDecExternal(tlVm* vm);

tlVm* tlVmCurrent(tlTask* task);
tlWorker* tlWorkerCurrent(tlTask* task);
tlFrame* tlTaskCurrentFrame(tlTask* task);

// ** running code **

// runs until all tasks are done
tlTask* tlVmEval(tlVm* vm, tlHandle v);
tlTask* tlVmEvalCall(tlVm* vm, tlHandle fn, ...);
tlTask* tlVmEvalInit(tlVm* vm, tlArgs* as);
tlTask* tlVmEvalInitBuffer(tlVm* vm, tlBuffer* buf, const char* name, tlArgs* args);
tlTask* tlVmEvalCode(tlVm* vm, tlString* code, tlString* file, tlArgs* as);
tlTask* tlVmEvalFile(tlVm* vm, tlString* file, tlArgs* as);

// setup a single task to eval something, returns immedately
tlHandle tlEval(tlTask* task, tlHandle v);
tlHandle tlEvalCall(tlHandle fn, ...);
tlTask* tlEvalCode(tlVm* vm, tlString* code, tlArgs* as);

// reading status from tasks
bool tlTaskIsDone(tlTask* task);
tlHandle tlTaskGetThrowValue(tlTask* task);
tlHandle tlTaskGetValue(tlTask* task);
bool tlTaskHasError(tlTask* task);
tlHandle tlTaskValue(tlTask* task);

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
typedef tlHandle(*tlNativeCb)(tlTask*, tlArgs*);
tlNative* tlNATIVE(tlNativeCb cb, const char* name);
tlNative* tlNativeNew(tlNativeCb cb, tlSym name);

// bulk create native functions and globals (don't overuse these)
typedef struct { const char* name; tlNativeCb cb; } tlNativeCbs;
void tl_register_natives(const tlNativeCbs* cbs);
void tl_register_global(const char* name, tlHandle v);

// task management
tlTask* tlTaskNew(tlVm* vm, tlObject* locals);
tlVm* tlTaskGetVm(tlTask* task);

// task internals ... cleanup?
tlTask* tlTaskFromEntry(lqentry* entry);
lqentry* tlTaskGetEntry(tlTask* task);

// stack reification
// native frame activation/resume signature
// if res is set, a call returned a regular result
// otherwise, if throw is set, a call has thrown a value (likely a tlError)
// if both are null, we are "unwinding" the stack for a jump (return/continuation ...)
typedef tlHandle(*tlResumeCb)(tlTask* task, tlFrame* frame, tlHandle res, tlHandle throw);

// mark task as waiting; returns an array of tasks incase of deadlock
tlArray* tlTaskWaitFor(tlTask* task, tlHandle on);

// mark task as ready, will add to run queue, might be picked up immediately
void tlTaskReady(tlTask* task);

// mark the current task as waiting for external event
tlTask* tlTaskWaitExternal(tlTask* task);
void tlTaskReadyExternal(tlTask* task);
void tlTaskSetValue(tlTask* task, tlHandle h);

// throws value, if it is a o with a stack, it will be filled in witha stacktrace
tlHandle tlTaskError(tlTask* task, tlHandle err);

// throw errors
tlHandle tlErrorThrow(tlTask* task, tlHandle msg);
tlHandle tlArgumentErrorThrow(tlTask* task, tlHandle msg);
tlHandle tlUndefMsg(tlString* msg);

#define TL_THROW(f, x...) do {\
    char _s[2048]; snprintf(_s, sizeof(_s), f, ##x);\
    return tlErrorThrow(task, tlStringFromCopy(_s, 0)); } while (0)
#define TL_THROW_NORETURN(f, x...) do {\
    char _s[2048]; snprintf(_s, sizeof(_s), f, ##x);\
    tlErrorThrow(task, tlStringFromCopy(_s, 0)); } while (0)
#define TL_ILLARG(f, x...) do {\
    char _s[2048]; snprintf(_s, sizeof(_s), f, ##x);\
    return tlArgumentErrorThrow(task, tlStringFromCopy(_s, 0)); } while (0)
#define TL_THROW_SET(f, x...) do {\
    char _s[2048]; snprintf(_s, sizeof(_s), f, ##x);\
    tlTaskErrorTake(task, strdup(_s)); } while (0)
#define TL_UNDEF(f, x...) do {\
    char _s[2048]; snprintf(_s, sizeof(_s), f, ##x);\
    return tlUndefMsg(tlStringFromCopy(_s, 0)); } while (0)

#define TL_TARGET(t, n) t* n = t##Cast(tlArgsTarget(args)); if (!n) TL_THROW("unexpected receiver")

// args
tlArgs* tlArgsEmpty();
tlArgs* tlArgsNew(int size);
tlArgs* tlArgsNewNamed(int size, bool isMethod, bool hasNames);
tlArgs* tlArgsNewFromListMap(tlList* list, tlObject* map);
tlArgs* tlArgsNewNames(int size, bool hasNames, bool isMethod);
void tlArgsSet_(tlArgs* args, int at, tlHandle v);
void tlArgsSetNames_(tlArgs* args, tlList* names, int nsize);
tlArgs* tlCallFrom(tlHandle fn, ...);

void tlArgsSetFn_(tlArgs* args, tlHandle fn);
void tlArgsSetTarget_(tlArgs* args, tlHandle target);
void tlArgsSetMethod_(tlArgs* args, tlHandle msg);

tlHandle tlArgsTarget(tlArgs* args);
tlSym tlArgsMethod(tlArgs* args);
tlHandle tlArgsBlock(tlArgs* args);

tlHandle tlArgsFn(tlArgs* args);
int tlArgsSize(tlArgs* args);
int tlArgsNamedSize(tlArgs* args);
int tlArgsRawSize(tlArgs* args);
tlHandle tlArgsGet(tlArgs* args, int at);
tlHandle tlArgsGetNamed(tlArgs* args, tlSym name);
tlHandle tlArgsGetRaw(tlArgs* args, int at);


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
tlObject* tlClassObjectFrom(const char* n1, tlNativeCb fn1, ...);

tlWorker* tlWorkerNew(tlVm* vm);
void tlWorkerDelete(tlWorker* worker);

// when creating your own worker, run this ... (not implemented yet)
void tlWorkerRun(tlWorker* worker);

// TODO remove?
tlHandle tlEvalArgsFn(tlTask* task, tlArgs* args, tlHandle fn);
tlHandle tlEvalArgsTarget(tlTask* task, tlArgs* args, tlHandle target, tlHandle fn);

typedef struct tlLock tlLock;

bool tlLockIs(tlHandle v);
tlLock* tlLockAs(tlHandle v);
tlLock* tlLockCast(tlHandle v);
tlTask* tlLockOwner(tlLock* lock);

typedef const char*(*tltoStringFn)(tlHandle v, char* buf, int size);
typedef uint32_t(*tlHashFn)(tlHandle from);
typedef int(*tlEqualsFn)(tlHandle left, tlHandle right);
typedef tlHandle(*tlCompareFn)(tlHandle left, tlHandle right);
typedef size_t(*tlByteSizeFn)(tlHandle value);
typedef void(*tlFinalizer)(tlHandle value);
typedef tlHandle(*tlCallFn)(tlTask* task, tlArgs* args);
typedef tlHandle(*tlRunFn)(tlTask* task, tlHandle fn, tlArgs* args);
typedef tlHandle(*tlSendFn)(tlTask* task, tlArgs* args, bool safe);

// a kind describes a hotel value for the vm (not at the language level)
// a kind can have a tlObject* klass, which represents its class from the language level
// fields with a null value is usually perfectly fine
struct tlKind {
    const char* name;  // general name of the value
    int index;         // a sorting index, larger values are sorted before other kinds
    tltoStringFn toString; // a toString "printer" function

    tlHashFn hash;     // return a hash value, must be stable
    tlEqualsFn equals; // return left == right, required if hash is not null
    tlCompareFn cmp;   // return tlCOMPARE(left - right) or undefined

    tlByteSizeFn size; // return memory use in bytes, e.g. `return sizeof(tlTask)`
    tlFinalizer finalizer; // if non null, will be called just before the value is gc'ed

    tlCallFn call;     // if called as a function, before arguments are evaluated
    tlRunFn run;       // if called as a function, after arguments are evaluated

    bool locked;       // if the object has a lock, the task will aqcuire the lock
    tlSendFn send;     // if send a message args.this and args.msg are filled in
    tlObject* klass;   // if send a message, lookup args.msg here and eval its field

    tlClass* cls;      // the class of this value
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


// ** buffer **

tlBuffer* tlBufferNew();
tlBuffer* tlBufferFromFile(const char* file);

int tlBufferSize(tlBuffer* buf);
const char* tlBufferData(tlBuffer* buf);

int tlBufferFind(tlBuffer* buf, const char* text, int len);
int tlBufferRead(tlBuffer* buf, char* to, int count);
int tlBufferReadSkip(tlBuffer* buf, int count);
uint8_t tlBufferReadByte(tlBuffer* buf);

int tlBufferWrite(tlBuffer* buf, const char* from, int count);
char* tlBufferWriteData(tlBuffer* buf, int len);
int tlBufferWriteByte(tlBuffer* buf, const char byte);
void tlBufferDidWrite(tlBuffer* buf, int count);
int tlBufferCanWrite(tlBuffer* buf);

tlBin* tlBinNew(int len);
tlBin* tlBinFromBufferTake(tlBuffer* buf);
tlBin* tlBinFromCopy(const char* data, int len);
int tlBinSize(tlBin* bin);
const char* tlBinData(tlBin* bin);
int tlBinGet(tlBin* bin, int at);
void tlBinSet_(tlBin* bin, int at, char byte);

tlArray* tlArrayNew();
int tlArraySize(tlArray* array);
tlHandle tlArrayGet(tlArray* array, int at);
tlArray* tlArrayAdd(tlArray* array, tlHandle v);
tlList* tlArrayToList(tlArray* array);

tlHashMap* tlHashMapNew();
tlHandle tlHashMapGet(tlHashMap* map, tlHandle k);
tlHandle tlHashMapSet(tlHashMap* map, tlHandle k, tlHandle v);
tlObject* tlHashMapToObject(tlHashMap* map);
tlMap* tlHashMapToMap(tlHashMap* map);

tlQueue* tlQueueNew(int buffersize);
void tlQueueClose(tlQueue* queue);
// using these queue methods might put the passed in task to waiting, handle with care
tlHandle tlQueueAdd(tlQueue* queue, tlTask* task, tlArgs* args);
tlHandle tlQueueGet(tlQueue* queue, tlTask* task);
tlHandle tlQueuePoll(tlQueue* queue, tlTask* task);

// ** running code **

tlBModule* tlBModuleFromBuffer(tlBuffer* buf, tlString* name, const char** error);
tlBModule* tlBModuleLink(tlBModule* mod, tlObject* env, const char** error);
tlTask* tlBModuleCreateTask(tlVm* vm, tlBModule* mod, tlArgs* args);
void tlVmRun(tlVm* vm, tlTask* task);


// ** not exactly public **

static inline void set_kptr(tlHandle v, intptr_t kind) { ((tlHead*)v)->kind = kind; }
static inline void set_kind(tlHandle v, tlKind* kind) { ((tlHead*)v)->kind = (intptr_t)kind; } // TODO take care of flags?

static inline bool tlflag_isset(tlHandle v, unsigned flag) { return get_kptr(v) & flag; }
static inline void tlflag_clear(tlHandle v, unsigned flag) { set_kptr(v, get_kptr(v) & ~flag); }
static inline void tlflag_set(tlHandle v, unsigned flag) {
    assert(flag <= 0x7);
    set_kptr(v, get_kptr(v) | flag);
}

#endif // _tl_h_

