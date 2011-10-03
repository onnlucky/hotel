// simple text type, wraps a char array with size and tag

#include "trace-on.h"

static tlClass tlTextClass;

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
    text->head.klass = &tlTextClass;
    //text->head.flags |= TL_FLAG_NOFREE;
    text->data = s;
    return text;
}

tlText* tltext_from_take(tlTask* task, char* s) {
    tlText* text = task_alloc_priv(task, TLText, 0, 4);
    text->head.klass = &tlTextClass;
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
    text->head.klass = &tlTextClass;
    text->size = tlINT(size);
    return text;
}

static void tltext_free(tlText *text) {
    assert(tltext_is(text));
    //if (text->head.flags & TL_FLAG_NOFREE) return;
    //free((char*)text->data);
}

const char * tltext_data(tlText *text) {
    assert(tltext_is(text));
    return text->data;
}
const char * tlTextData(tlText *text) {
    assert(tlTextIs(text));
    return text->data;
}

int tltext_size(tlText* text) {
    assert(tltext_is(text));
    if (!text->bytes) {
        text->bytes = tlINT(strlen(tltext_data(text)));
    }
    return tl_int(text->bytes);
}
int tlTextSize(tlText* text) { return tltext_size(text); }

tlText* tltext_sub(tlTask* task, tlText* from, int first, int size) {
    if (tlTextSize(from) == size) return from;
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

INTERNAL tlPause* _TextSize(tlTask* task, tlArgs* args) {
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    TL_RETURN(tlINT(tltext_size(text)));
}

INTERNAL tlPause* _TextSearch(tlTask* task, tlArgs* args) {
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* find = tlTextCast(tlArgsAt(args, 0));
    if (!find) TL_THROW("expected a Text");
    const char* p = strstr(tlTextData(text), tlTextData(find));
    if (!p) TL_RETURN(tlNull);
    TL_RETURN(tlINT(p - tlTextData(text)));
}

INTERNAL tlPause* _TextSlice(tlTask* task, tlArgs* args) {
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    int size = tltext_size(text);
    int first = tl_int_or(tlargs_get(args, 0), 0);
    int last = tl_int_or(tlargs_get(args, 1), size);

    trace("%d %d (%s)%d", first, last, text->data, size);

    if (first < 0) first = size + first;
    if (first < 0) TL_RETURN(v_empty_text);
    if (first >= size) TL_RETURN(v_empty_text);
    if (last <= 0) last = size + last;
    if (last < first) TL_RETURN(v_empty_text);

    trace("%d %d (%s)%d", first, last, text->data, size);

    TL_RETURN(tltext_sub(task, text, first, last - first));
}

static tlClass tlTextClass = {
    .name = "text",
    .map = null,
    .send = null
};

static void text_init() {
    tlTextClass.map = tlClassMapFrom(
            "size", _TextSize,
            "search", _TextSearch,
            "slice", _TextSlice,
            null
    );
    v_empty_text = tlTEXT("");
}

