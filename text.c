// ** simple text type, wraps a char array with size and tag **

#include "trace-off.h"

static tText* v_empty_text;

static void text_init() {
    v_empty_text = tTEXT("");
}

// TODO short strings should be "inline" saves finalizer
struct tText {
    tHead head;
    const char* ptr;
    tInt bytes;
    tInt size;
    tInt hash;
};

tText* ttext_from_static(const char* s) {
    tText* text = task_alloc_priv(null, TText, 0, 4);
    text->head.flags |= T_FLAG_NOFREE;
    text->ptr = s;
    return text;
}

tText* ttext_from_take(tTask* task, char* s) {
    tText* text = task_alloc_priv(task, TText, 0, 4);
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
    if (text->head.flags & T_FLAG_NOFREE) return;
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

tText* tvalue_to_text(tTask* task, tValue v) {
    if (ttext_is(v)) return v;
    if (!tref_is(v)) return ttext_from_copy(task, t_str(v));
    warning("to_text not implemented yet: %s", t_str(v));
    return tTEXT("<ERROR.to-text>");
}

