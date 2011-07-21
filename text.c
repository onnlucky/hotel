// simple text type, wraps a char array with size and tag

#include "trace-off.h"

static tlText* v_empty_text;

// TODO short strings should be "inline" saves finalizer
struct tlText {
    tlHead head;
    const char* data;
    tlInt bytes;
    tlInt size;
    tlInt hash;
};

tlText* tltext_empty() { return v_empty_text; }

tlText* tltext_from_static(const char* s) {
    tlText* text = task_alloc_priv(null, TLText, 0, 4);
    text->head.flags |= TL_FLAG_NOFREE;
    text->data = s;
    return text;
}

tlText* tltext_from_take(tlTask* task, char* s) {
    tlText* text = task_alloc_priv(task, TLText, 0, 4);
    text->data = s;
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
    free((char*)text->data);
}

const char * tltext_bytes(tlText *text) {
    assert(tltext_is(text));
    return text->data;
}

int tltext_size(tlText* text) {
    assert(tltext_is(text));
    if (!text->bytes) {
        text->bytes = tlINT(strlen(tltext_bytes(text)));
    }
    return tl_int(text->bytes);
}

tlText* tltext_sub(tlTask* task, tlText* from, int first, int size) {
    char* data = malloc(size + 1);
    if (!data) return null;
    memcpy(data, from->data + first, size);
    data[size] = 0;

    return tltext_from_take(task, data);
}

tlText* tlvalue_to_text(tlTask* task, tlValue v) {
    if (tltext_is(v)) return v;
    if (!tlref_is(v)) return tltext_from_copy(task, tl_str(v));
    warning("to_text not implemented yet: %s", tl_str(v));
    return tlTEXT("<ERROR.to-text>");
}

static tlValue _text_size(tlTask* task, tlArgs* args, tlRun* run) {
    tlText* text = tltext_cast(tlargs_get(args, 0));
    if (!text) return tlNull;
    return tlINT(tltext_size(text));
}
static tlValue _text_slice(tlTask* task, tlArgs* args, tlRun* run) {
    tlText* text = tltext_cast(tlargs_get(args, 0));
    if (!text) return tlNull;
    int size = tltext_size(text);
    int first = tl_int_or(tlargs_get(args, 1), 0);
    int last = tl_int_or(tlargs_get(args, 2), size);

    trace("%d %d (%s)%d", first, last, text->data, size);

    if (first < 0) first = size + first;
    if (first < 0) return v_empty_text;
    if (first >= size) return v_empty_text;
    if (last <= 0) last = size + last;
    if (last < first) return v_empty_text;

    return tltext_sub(task, text, first, last - first);
}

static const tlHostFunctions __text_functions[] = {
    { "_text_size", _text_size },
    { "_text_slice", _text_slice },
    { 0, 0 }
};

static void text_init() {
    v_empty_text = tlTEXT("");
    tl_register_functions(__text_functions);
}

