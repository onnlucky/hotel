#include "platform.h"

#include "value.c"
#include "text.c"
#include "list.c"

#include "bytecode.c"
#include "eval.c"

#include "buffer.c"

#include "compiler.c"
#include "parser.c"

const size_t type_to_size[] = {
    -1,
    -1, -1, -1, -1, -1, -1/*sizeof(tFloat)*/,
    sizeof(tList), -1/*sizeof(tMap)*/, sizeof(tEnv),
    sizeof(tText), sizeof(tMem),
    sizeof(tCall), sizeof(tThunk), sizeof(tResult),
    sizeof(tFun), sizeof(tCFun),
    sizeof(tCode), sizeof(tFrame), sizeof(tArgs), sizeof(tEvalFrame),
    sizeof(tTask), -1, -1 /*sizeof(tVar), sizeof(tCTask)*/
};

#define _BUF_COUNT 8
#define _BUF_SIZE 128
static char** _str_bufs;
static char* _str_buf;
static int _str_buf_at = -1;

const char* t_str(tValue v) {
    if (_str_buf_at == -1) {
        _str_buf_at = 0;
        _str_bufs = calloc(_BUF_COUNT, sizeof(_str_buf));
        for (int i = 0; i < _BUF_COUNT; i++) _str_bufs[i] = malloc(_BUF_SIZE);
    }
    _str_buf_at = (_str_buf_at + 1) % _BUF_COUNT;
    _str_buf = _str_bufs[_str_buf_at];

    switch (t_type(v)) {
        case TText: return ttext_bytes(ttext_as(v));
        case TInt: snprintf(_str_buf, _BUF_SIZE, "%d", t_int(v)); return _str_buf;
        default: return t_type_str(v);
    }
}

int main() {
    // assert assumptions on memory layout, pointer size etc
    assert(sizeof(tHead) == sizeof(intptr_t));
    tHead h; h.moved = 0; h.flags = 1;
    assert(h.moved == (void*)1);
    h.moved = &h; assert((h.flags & 3) == 0);

    print("    field size: %zd", sizeof(tValue));
    //print(" call overhead: %zd (%zd)", sizeof(tCall), sizeof(tCall)/sizeof(tValue));
    //print("frame overhead: %zd (%zd)", sizeof(tFrame), sizeof(tFrame)/sizeof(tValue));
    //print(" task overhead: %zd (%zd)", sizeof(tTask), sizeof(tTask)/sizeof(tValue));

    list_init();
    text_init();

    tValue v = compile(tTEXT("print(43)"));
    if (t_type(v) == TCode) {
        tcode_print(tcode_as(v));
    }
    print("done");
}

