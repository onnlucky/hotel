// simple text type, wraps a char array with size and tag

#include "trace-off.h"

static tlText* v_empty_text;

static void tlext_init() {
    v_empty_text = tlTEXT("");
}

// TODO short strings should be "inline" saves finalizer
struct tlText {
    tlHead head;
    const char* ptr;
    tlInt bytes;
    tlInt size;
    tlInt hash;
};

tlText* tltext_empty() { return v_empty_text; }

tlText* tltext_from_static(const char* s) {
    tlText* text = task_alloc_priv(null, TLText, 0, 4);
    text->head.flags |= TL_FLAG_NOFREE;
    text->ptr = s;
    return text;
}

tlText* tltext_from_take(tlTask* task, char* s) {
    tlText* text = task_alloc_priv(task, TLText, 0, 4);
    text->ptr = s;
    return text;
}

tlText* tltext_from_copy(tlTask* task, const char* s) {
    size_t size = strlen(s);
    assert(size < TL_MAX_INT);
    char *d = malloc(size + 1);
    assert(d);
    memcpy(d, s, size + 1);

    tlText* text = tltext_from_take(task, d);
    text->size = tlINT(size);
    return text;
}

static void tltext_free(tlText *text) {
    assert(tltext_is(text));
    if (text->head.flags & TL_FLAG_NOFREE) return;
    free((char*)text->ptr);
}

const char * tltext_bytes(tlText *text) {
    assert(tltext_is(text));
    return text->ptr;
}

int tltext_size(tlText* text) {
    assert(tltext_is(text));
    if (!text->bytes) {
        text->bytes = tlINT(strlen(tltext_bytes(text)));
    }
    return tl_int(text->bytes);
}

tlText* tlvalue_to_text(tlTask* task, tlValue v) {
    if (tltext_is(v)) return v;
    if (!tlref_is(v)) return tltext_from_copy(task, tl_str(v));
    warning("to_text not implemented yet: %s", tl_str(v));
    return tlTEXT("<ERROR.to-text>");
}

