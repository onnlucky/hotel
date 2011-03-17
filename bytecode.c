#include "trace-off.h"

enum {
    OEND=0,
    OARG_EVAL, OARG_EVAL_DEFAULT, OARG_LAZY, OARGS_REST, OARGS,
    ORESULT, ORESULT_REST,
    OBIND,
    OGETTEMP, OGETDATA, OGETENV,
    OSETTEMP, OSETENV,
    OCGETTEMP, OCGETDATA, OCGETENV,
    OCALL, OEVAL, OGOTO,
    OLAST
};

const char* const ops_to_str[] = {
    "END",
    "ARG_EVAL", "ARG_EVAL_DEFAULT", "ARG_LAZY", "ARGS_REST", "ARGS",
    "RESULT", "RESULT_REST",
    "BIND",
    "GETTEMP", "GETDATA", "GETENV",
    "SETTEMP", "SETENV",
    "CGETTEMP", "CGETDATA", "CGETENV",
    "CALL", "EVAL", "GOTO"
};

typedef struct tCode tCode;
struct tCode {
    tHead head;
    intptr_t temps;
    const uint8_t* ops;
    tValue localkeys;
    tValue data[];
};
bool tcode_is(tValue v) { return t_type(v) == TCode; }
tCode* tcode_as(tValue v) { assert(tcode_is(v)); return (tCode*)v; }

tCode* tcode_new_global(int size, int temps) {
    trace("%d", size);
    tCode* code = (tCode*)global_alloc(TCode, size);
    code->temps = temps;
    code->localkeys = 0;
    return code;
}

tCode* tcode_new(tTask* task, int size, int temps, tList* localkeys, tList* data) {
    trace("%d", tlist_size(data));
    tCode* code = task_alloc(task, TCode, tlist_size(data));
    code->temps = temps;
    code->ops = malloc(size);
    code->localkeys = localkeys;
    memcpy(code->data, data->data, sizeof(tValue)*tlist_size(data));
    return code;
}

char* tcode_bytes_(tCode* code) {
    return (char*)code->ops;
}

void tcode_print(tCode* code) {
    print("temps=%zd data.size=%d locals=%d",
          code->temps, code->head.size, code->localkeys?tlist_size(code->localkeys):0);
    const uint8_t *ops = code->ops;
    while (*ops) {
        uint8_t op = *(ops++);
        const char* name = ops_to_str[op];

        switch (op) {
        case OARG_EVAL: case OARG_LAZY: case OARGS_REST: case ORESULT: case ORESULT_REST:
            print("%s %d = %s", name, *ops, t_str(code->data[*ops]));
            ops++; break;
        case OARG_EVAL_DEFAULT:
            print("%s %d %d = %s or %s", name, *ops, *(ops+1),
                  t_str(code->data[*ops]), t_str(code->data[*(ops+1)]));
            ops++; ops++; break;
        case OCALL:
            print("%s %d", name, *(ops++)); break;
        case OGETTEMP: case OCGETTEMP: case OSETTEMP:
            print("%s %d", name, *(ops++)); break;
        case OGETDATA: case OCGETDATA:
            print("%s %d = %s", name, *ops, t_str(code->data[*ops]));
            ops++; break;
        case OGETENV: case OCGETENV:
            print("%s %d = %s", name, *ops, t_str(code->data[*ops]));
            ops++; break;
        default:
            print("%s", ops_to_str[op]);
        }
    }
    print("%s", ops_to_str[*ops]);
}
