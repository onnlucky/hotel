// author: Onne Gorter, license: MIT (see license.txt)
// simple text type, wraps a char array with size and tag

#include "trace-off.h"

static tlClass _tlTextClass;
tlClass* tlTextClass = &_tlTextClass;

static tlText* _tl_emptyText;

// TODO short strings should be "inline" saves finalizer
struct tlText {
    tlHead head;
    const char* data;
    int len;
};

tlText* tlTextEmpty() { return _tl_emptyText; }

tlText* tlTextFromStatic(const char* s, int len) {
    trace("%s", s);
    tlText* text = tlAlloc(null, tlTextClass, sizeof(tlText));
    text->data = s;
    text->len = (len > 0)?len:strlen(s);
    return text;
}

tlText* tlTextFromTake(tlTask* task, char* s, int len) {
    trace("%s", s);
    tlText* text = tlAlloc(task, tlTextClass, sizeof(tlText));
    text->data = s;
    text->len = (len > 0)?len:strlen(s);
    return text;
}

tlText* tlTextFromCopy(tlTask* task, const char* s, int len) {
    trace("%s", s);
    if (len <= 0) len = strlen(s);
    //assert(len < TL_MAX_INT);
    char *data = malloc(len + 1);
    if (!data) return null;
    memcpy(data, s, len + 1);
    return tlTextFromTake(task, data, len);
}

const char* tlTextData(tlText *text) {
    assert(tlTextIs(text));
    return text->data;
}

int tlTextSize(tlText* text) {
    assert(tlTextIs(text));
    return text->len;
}

tlText* tlTextSub(tlTask* task, tlText* from, int first, int size) {
    if (tlTextSize(from) == size) return from;
    char* data = malloc(size + 1);
    if (!data) return null;
    memcpy(data, from->data + first, size);
    data[size] = 0;

    return tlTextFromTake(task, data, size);
}

tlText* tlTextCat(tlTask* task, tlText* left, tlText* right) {
    int size = tlTextSize(left) + tlTextSize(right);
    char* data = malloc(size + 1);
    if (!data) return null;
    memcpy(data, left->data, tlTextSize(left));
    memcpy(data + tlTextSize(left), right->data, tlTextSize(right));
    data[size] = 0;
    return tlTextFromTake(task, data, size);
}

// generated by parser for "foo: $foo" etc...
// TODO args.map(_.toText) do it more correctly invoking toText methods
INTERNAL tlValue _Text_cat(tlTask* task, tlArgs* args) {
    int argc = tlArgsSize(args);
    trace("%d", argc);
    tlList* list = null;
    int size = 0;
    for (int i = 0; i < argc; i++) {
        tlValue v = tlArgsGet(args, i);
        trace("%d: %s", i, tl_str(v));
        if (tlTextIs(v)) {
            size += tlTextAs(v)->len;
        } else {
            if (!list) list = tlListNew(task, argc);
            tlText* text = tlTextFromCopy(task, tl_str(tlArgsGet(args, i)), 0);
            size += text->len;
            tlListSet_(list, i, text);
        }
    }

    char* data = malloc(size + 1);
    if (!data) return null;
    size = 0;
    for (int i = 0; i < argc; i++) {
        const char* str;
        int len;
        tlValue v = tlArgsGet(args, i);
        if (tlTextIs(v)) {
            str = tlTextAs(v)->data;
            len = tlTextAs(v)->len;
        } else {
            tlText* text = tlTextAs(tlListGet(list, i));
            str = text->data;
            len = text->len;
        }
        memcpy(data + size, str, len);
        size += len;
    }
    data[size] = 0;
    return tlTextFromTake(task, data, size);
}

INTERNAL tlValue _text_size(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    return tlINT(tlTextSize(text));
}

INTERNAL tlValue _text_search(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* find = tlTextCast(tlArgsGet(args, 0));
    if (!find) TL_THROW("expected a Text");
    const char* p = strstr(tlTextData(text), tlTextData(find));
    if (!p) return tlNull;
    return tlINT(p - tlTextData(text));
}

INTERNAL tlValue _text_cat(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* add = tlTextCast(tlArgsGet(args, 0));
    if (!add) TL_THROW("arg must be a Text");

    return tlTextCat(task, text, add);
}

INTERNAL tlValue _text_slice(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    int size = tlTextSize(text);
    int first = tl_int_or(tlArgsGet(args, 0), 0);
    int last = tl_int_or(tlArgsGet(args, 1), size);

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

INTERNAL tlValue _text_startsWith(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* start = tlTextCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a Text");
    int from = tl_int_or((tlArgsGet(args, 1)), 0);


    int textsize = tlTextSize(text);
    int startsize = tlTextSize(start);
    if (textsize + from < startsize) return tlFalse;

    int r = strncmp(tlTextData(text) + from, tlTextData(start), startsize);
    return tlBOOL(r == 0);
}

INTERNAL tlValue _text_endsWith(tlTask* task, tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* start = tlTextCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a Text");

    int textsize = tlTextSize(text);
    int startsize = tlTextSize(start);
    if (textsize < startsize) return tlFalse;

    int r = strncmp(tlTextData(text) + textsize - startsize, tlTextData(start), startsize);
    return tlBOOL(r == 0);
}

INTERNAL const char* textToText(tlValue v, char* buf, int size) {
    return tlTextData(tlTextAs(v));
}
static tlClass _tlTextClass = {
    .name = "text",
    .toText = textToText,
};

static void text_init() {
    _tlTextClass.map = tlClassMapFrom(
            "size", _text_size,
            "startsWith", _text_startsWith,
            "endsWith", _text_endsWith,
            "search", _text_search,

            "slice", _text_slice,
            "cat", _text_cat,
            null
    );
    _tl_emptyText = tlTEXT("");
}

