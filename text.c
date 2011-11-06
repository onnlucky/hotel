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

tlText* tlTextCat(tlTask* task, tlText* left, tlText* right) {
    int size = tlTextSize(left) + tlTextSize(right);
    char* data = malloc(size + 1);
    if (!data) return null;
    memcpy(data, left->data, tlTextSize(left));
    memcpy(data + tlTextSize(left), right->data, tlTextSize(right));
    data[size] = 0;
    return tlTextNewTake(task, data);
}

INTERNAL tlValue _TextSize(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    return tlINT(tlTextSize(text));
}

INTERNAL tlValue _TextSearch(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* find = tlTextCast(tlArgsAt(args, 0));
    if (!find) TL_THROW("expected a Text");
    const char* p = strstr(tlTextData(text), tlTextData(find));
    if (!p) return tlNull;
    return tlINT(p - tlTextData(text));
}

INTERNAL tlValue _TextCat(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* add = tlTextCast(tlArgsAt(args, 0));
    if (!add) TL_THROW("arg must be a Text");

    return tlTextCat(task, text, add);
}

INTERNAL tlValue _TextSlice(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    int size = tlTextSize(text);
    int first = tl_int_or(tlArgsAt(args, 0), 0);
    int last = tl_int_or(tlArgsAt(args, 1), size);

    trace("%d %d (%s)%d", first, last, text->data, size);

    if (first < 0) first = size + first;
    if (first < 0) return tlTextEmpty();
    if (first >= size) return tlTextEmpty();
    if (last <= 0) last = size + last;
    if (last < first) return tlTextEmpty();

    trace("%d %d (%s)%d", first, last, text->data, size);

    return tlTextSub(task, text, first, last - first);
}

static int intmin(int left, int right) { return (left<right)?left:right; }

INTERNAL tlValue _TextStartsWith(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* start = tlTextCast(tlArgsAt(args, 0));
    if (!start) TL_THROW("arg must be a Text");

    int textsize = tlTextSize(text);
    int startsize = tlTextSize(start);
    if (textsize < startsize) return tlFalse;

    int r = strncmp(tlTextData(text), tlTextData(start), startsize);
    return tlBOOL(r == 0);
}

INTERNAL tlValue _TextEndsWith(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* start = tlTextCast(tlArgsAt(args, 0));
    if (!start) TL_THROW("arg must be a Text");

    int textsize = tlTextSize(text);
    int startsize = tlTextSize(start);
    if (textsize < startsize) return tlFalse;

    int r = strncmp(tlTextData(text) + textsize - startsize, tlTextData(start), startsize);
    return tlBOOL(r == 0);
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
            "startsWith", _TextStartsWith,
            "endsWith", _TextEndsWith,
            "search", _TextSearch,

            "slice", _TextSlice,
            "cat", _TextCat,
            null
    );
    _tl_emptyText = tlTEXT("");
}

