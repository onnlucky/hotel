// ** this is how all tl values look in memory **

typedef void* tValue;
typedef struct tHead tHead;
struct tHead {
    uint8_t flags; // must be least significant byte
    uint8_t type;
    uint16_t size;
};
struct tFields {
    tHead head;
    tValue data[];
};

#define MAX_DATA_SIZE 65530

typedef tValue tSym;
typedef tValue tInt;
typedef struct tText tText;
typedef struct tMem tMem;

typedef struct tFields tFields;
typedef struct tFields tList;
typedef struct tFields tResult;

typedef struct tTask tTask;
typedef struct tArgs tArgs;

// ** all known types **
typedef enum {
    TInvalid = 0,
    TUndefined, TNull, TBool, TSym, TInt, TFloat,
    TList, TMap, TEnv,
    TText, TMem,
    TCall, TThunk, TResult,
    TFun, TCFun,
    TCode, TFrame, TArgs, TEvalFrame,
    TTask, TVar, TCTask,
    TLAST
} tType;

const size_t type_to_size[];
const int8_t type_to_private[] = {
    -1,
    -1, -1, -1, -1, -1, 1,
    0, 0, 0,
    -1, -1,
    0, 0, 0,
    0, 1,
    2, 2, 0, 0,
    0, 0, 1
};
const char* const type_to_str[] = {
    "invalid",
    "Undefined", "Null", "Bool", "Sym", "Int", "Float",
    "List", "Map", "Env",
    "Text", "Mem",
    "Call", "Thunk", "Result",
    "Fun", "CFun",
    "Code", "Frame", "Args", "EvalFrame",
    "Task", "Var", "CTask"
};

bool ttag_is(tValue v) {
    return ((intptr_t)(v) & 3) != 0 || (intptr_t)(v)  < 1024;
}
bool tref_is(tValue v) {
    return ((intptr_t)(v) & 3) == 0 && (intptr_t)(v) >= 1024;
}
static inline tHead* t_head(tValue v) {
    assert(tref_is(v)); return (tHead*)v;
}

const tValue tUndef = (tHead*)(1 << 2);
const tValue tNull  = (tHead*)(2 << 2);
const tValue tFalse = (tHead*)(3 << 2);
const tValue tTrue  = (tHead*)(4 << 2);

tType t_type(tValue v) {
    if (tref_is(v)) return t_head(v)->type;
    if ((intptr_t)v & 1) return TInt;
    if ((intptr_t)v & 2) return TSym;
    switch ((intptr_t)v) {
    case 1 << 2: return TUndefined;
    case 2 << 2: return TNull;
    case 3 << 2: return TBool;
    case 4 << 2: return TBool;
    }
    return TInvalid;
}

const char* t_type_str(tValue v) { return type_to_str[t_type(v)]; }
const char* t_str(tValue v);

bool tint_is(tValue v) { return t_type(v) == TInt; }

bool tsym_is(tValue v) { return t_type(v) == TSym; }
tSym tsym_as(tValue v) { assert(tsym_is(v)); return (tSym)v; }

bool ttext_is(tValue v) { return t_type(v) == TText; }
tText* ttext_as(tValue v) { assert(ttext_is(v)); return (tText*)v; }

bool tlist_is(tValue v) { return t_type(v) == TList; }
tList* tlist_as(tValue v) { assert(tlist_is(v)); return (tList*)v; }
tList* tlist_cast(tValue v) { if (!tlist_is(v)) return null; else return (tList*)v; }

bool ttask_is(tValue v) { return t_type(v) == TTask; }


// creating primitives

tValue tBOOL(bool i) { if (i) return tTrue; return tFalse; }
bool t_bool(tValue v) { return !(v == null || v == tUndef || v == tNull || v == tFalse); }

tValue tINT(int i) { return (tValue)((intptr_t)i << 2 | 1); }
int t_int(tValue v) {
    assert(tint_is(v));
    int i = (intptr_t)v;
    if (i < 0) return (i >> 2) | 0xC0000000;
    return i >> 2;
}


// creating other values

tValue global_alloc(uint8_t type, int size) {
    assert(type > 0 && type < TLAST);
    tHead* head = (tHead*)calloc(1, type_to_size[type] + sizeof(tValue)*size);
    head->flags = 1;
    head->type = type;
    head->size = size;
    return (tValue)head;
}

tValue task_alloc(tTask* task, uint8_t type, int size);

