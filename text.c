// ** simple text type, wraps a char array with size and tag **

#include "trace-off.h"

static tText* v_empty_text;

static void text_init() {
    v_empty_text = ttext_from_static(null, "");
}

struct tText {
    tHead head;
    const char* ptr;
    tInt bytes;
    tInt size;
    tInt hash;
};
#define T_TEXT_FIELDS (sizeof(tText)/sizeof(tValue) - 1)

tText* ttext_from_static(tTask* task, const char* s) {
    tText* text = task_alloc(task, TText, T_TEXT_FIELDS);
    text->head.flags = TPTR|TCONST;
    text->ptr = s;
    return text;
}
tText* ttext_from_take(tTask* task, char* s) {
    tText* text = task_alloc(task, TText, T_TEXT_FIELDS);
    text->head.flags = TPTR;
    text->ptr = s;
    return text;
}

tText* ttext_from_copy(tTask* task, const char* s) {
    size_t size = strlen(s);
    assert(size < T_MAX_INT);
    char *d = malloc(size + 1);
    assert(d);
    memcpy(d, s, size + 1);

    tText* text = ttext_from_take(task, d);
    text->size = tINT(size);
    return text;
}

static void ttext_free(tText *text) {
    assert(ttext_is(text));
    if (text->head.flags & TCONST) return;
    free((char*)text->ptr);
}

const char * ttext_bytes(tText *text) {
    assert(ttext_is(text));
    return text->ptr;
}

int ttext_size(tText* text) {
    assert(ttext_is(text));
    if (!text->bytes) {
        text->bytes = tINT(strlen(ttext_bytes(text)));
    }
    return t_int(text->bytes);
}

