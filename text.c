// simple text type, wraps a char array with size and tag

#include "trace-off.h"

static tlClass _tlTextClass;
tlClass* tlTextClass = &_tlTextClass;

static tlText* _tl_emptyText;

// TODO short strings should be "inline" saves finalizer
struct tlText {
    tlHead head;
    const char* data;
    tlInt bytes;
    tlInt size;
    tlInt hash;
};

tlText* tlTextEmpty() { return _tl_emptyText; }

tlText* tlTextFromStatic(const char* s) {
    trace("%s", s);
    tlText* text = tlAlloc(null, tlTextClass, sizeof(tlText));
    text->data = s;
    return text;
}

tlText* tlTextNewTake(tlTask* task, char* s) {
    trace("%s", s);
    tlText* text = tlAlloc(task, tlTextClass, sizeof(tlText));
    text->data = s;
    return text;
}

tlText* tlTextNewCopy(tlTask* task, const char* s) {
    trace("%s", s);
    size_t size = strlen(s);
    assert(size < TL_MAX_INT);
    char *data = malloc(size + 1);
    if (!data) return null;
    memcpy(data, s, size + 1);

    tlText* text = tlTextNewTake(task, data);
    text->size = tlINT(size);
    return text;
}

const char * tlTextData(tlText *text) {
    assert(tlTextIs(text));
    return text->data;
}

int tlTextSize(tlText* text) {
    assert(tlTextIs(text));
    if (!text->bytes) {
        text->bytes = tlINT(strlen(tlTextData(text)));
    }
    return tl_int(text->bytes);
}

tlText* tlTextSub(tlTask* task, tlText* from, int first, int size) {
    if (tlTextSize(from) == size) return from;
    char* data = malloc(size + 1);
    if (!data) return null;
    memcpy(data, from->data + first, size);
    data[size] = 0;

    return tlTextNewTake(task, data);
}

// TODO remove from here, move to eval, return a tlPause ...
tlText* tlvalue_to_text(tlTask* task, tlValue v) {
    if (tlTextIs(v)) return v;
    if (!tlref_is(v)) return tlTextNewCopy(task, tl_str(v));
    warning("to_text not implemented yet: %s", tl_str(v));
    return tlTEXT("<ERROR.to-text>");
}

INTERNAL tlPause* _TextSize(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    TL_RETURN(tlINT(tlTextSize(text)));
}

INTERNAL tlPause* _TextSearch(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* find = tlTextCast(tlArgsAt(args, 0));
    if (!find) TL_THROW("expected a Text");
    const char* p = strstr(tlTextData(text), tlTextData(find));
    if (!p) TL_RETURN(tlNull);
    TL_RETURN(tlINT(p - tlTextData(text)));
}

INTERNAL tlPause* _TextSlice(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    int size = tlTextSize(text);
    int first = tl_int_or(tlargs_get(args, 0), 0);
    int last = tl_int_or(tlargs_get(args, 1), size);

    trace("%d %d (%s)%d", first, last, text->data, size);

    if (first < 0) first = size + first;
    if (first < 0) TL_RETURN(tlTextEmpty());
    if (first >= size) TL_RETURN(tlTextEmpty());
    if (last <= 0) last = size + last;
    if (last < first) TL_RETURN(tlTextEmpty());

    trace("%d %d (%s)%d", first, last, text->data, size);

    TL_RETURN(tlTextSub(task, text, first, last - first));
}

const char* _TextToText(tlValue v, char* buf, int size) {
    snprintf(buf, size, "%s", tlTextData(tlTextAs(v))); return buf;
}

static tlClass _tlTextClass = {
    .name = "text",
    .toText = _TextToText,
};

static void text_init() {
    _tlTextClass.map = tlClassMapFrom(
            "size", _TextSize,
            "search", _TextSearch,
            "slice", _TextSlice,
            null
    );
    _tl_emptyText = tlTEXT("");
}

