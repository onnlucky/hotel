// author: Onne Gorter, license: MIT (see license.txt)
// simple text type, wraps a char array with size and tag

#include "trace-off.h"

static tlKind _tlTextKind;
tlKind* tlTextKind = &_tlTextKind;

static tlText* _tl_emptyText;

// TODO short strings should be "inline" saves finalizer
struct tlText {
    tlHead head;
    unsigned int hash;
    unsigned int len;
    const char* data;
};

tlText* tlTextEmpty() { return _tl_emptyText; }

tlText* tlTextFromStatic(const char* s, int len) {
    trace("%s", s);
    tlText* text = tlAlloc(tlTextKind, sizeof(tlText));
    text->data = s;
    text->len = (len > 0)?len:strlen(s);
    return text;
}

tlText* tlTextFromTake(char* s, int len) {
    trace("%s", s);
    tlText* text = tlAlloc(tlTextKind, sizeof(tlText));
    text->data = s;
    text->len = (len > 0)?len:strlen(s);
    return text;
}

tlText* tlTextFromCopy(const char* s, int len) {
    trace("%s", s);
    if (len <= 0) len = strlen(s);
    //assert(len < TL_MAX_INT);
    char *data = malloc_atomic(len + 1);
    if (!data) return null;
    memcpy(data, s, len);
    data[len] = 0;
    return tlTextFromTake(data, len);
}

const char* tlTextData(tlText *text) {
    assert(tlTextIs(text));
    return text->data;
}

int tlTextSize(tlText* text) {
    assert(tlTextIs(text));
    return text->len;
}

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
static unsigned int murmurhash2a(const void * key, int len) {
    const unsigned int seed = 33;
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    unsigned int l = len;
    const unsigned char * data = (const unsigned char *)key;
    unsigned int h = seed;
    while(len >= 4) {
        unsigned int k = *(unsigned int*)data;
        mmix(h,k);
        data += 4;
        len -= 4;
    }
    unsigned int t = 0;
    switch(len) {
        case 3: t ^= data[2] << 16;
        case 2: t ^= data[1] << 8;
        case 1: t ^= data[0];
    }
    mmix(h,t);
    mmix(h,l);
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

unsigned int tlTextHash(tlText* text) {
    assert(tlTextIs(text));
    if (text->hash) return text->hash;
    text->hash = murmurhash2a(text->data, text->len);
    if (!text->hash) text->hash = 1;
    return text->hash;
}
bool tlTextEquals(tlText* left, tlText* right) {
    if (left->len != right->len) return 0;
    if (left->hash && right->hash && left->hash != right->hash) return 0;
    return strcmp(left->data, right->data) == 0;
}
int tlTextCmp(tlText* left, tlText* right) {
    return strcmp(left->data, right->data);
}

tlText* tlTextSub(tlText* from, int first, int size) {
    if (tlTextSize(from) == size) return from;
    char* data = malloc_atomic(size + 1);
    if (!data) return null;
    memcpy(data, from->data + first, size);
    data[size] = 0;

    return tlTextFromTake(data, size);
}

tlText* tlTextCat(tlText* left, tlText* right) {
    int size = tlTextSize(left) + tlTextSize(right);
    char* data = malloc_atomic(size + 1);
    if (!data) return null;
    memcpy(data, left->data, tlTextSize(left));
    memcpy(data + tlTextSize(left), right->data, tlTextSize(right));
    data[size] = 0;
    return tlTextFromTake(data, size);
}

// generated by parser for "foo: $foo" etc...
// TODO args.map(_.toText) do it more correctly invoking toText methods
INTERNAL tlHandle _Text_cat(tlArgs* args) {
    int argc = tlArgsSize(args);
    trace("%d", argc);
    tlList* list = null;
    int size = 0;
    for (int i = 0; i < argc; i++) {
        tlHandle v = tlArgsGet(args, i);
        trace("%d: %s", i, tl_str(v));
        if (tlTextIs(v)) {
            size += tlTextAs(v)->len;
        } else {
            if (!list) list = tlListNew(argc);
            tlText* text = tlTextFromCopy(tl_str(tlArgsGet(args, i)), 0);
            size += text->len;
            tlListSet_(list, i, text);
        }
    }

    char* data = malloc_atomic(size + 1);
    if (!data) return null;
    size = 0;
    for (int i = 0; i < argc; i++) {
        const char* str;
        int len;
        tlHandle v = tlArgsGet(args, i);
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
    return tlTextFromTake(data, size);
}

INTERNAL tlHandle _text_toText(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    return text;
}
INTERNAL tlHandle _text_toSym(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    return tlSymFromText(text);
}
INTERNAL tlHandle _text_size(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    return tlINT(tlTextSize(text));
}
INTERNAL tlHandle _text_hash(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    // TODO this can overflow/underflow ... sometimes, need to fix
    return tlINT(tlTextHash(text));
}

INTERNAL tlHandle _text_find(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* find = tlTextCast(tlArgsGet(args, 0));
    if (!find) TL_THROW("expected a Text");
    int from = tl_int_or(tlArgsGet(args, 1), 0);

    if (from >= tlTextSize(text)) return tlNull;
    const char* p = strstr(tlTextData(text) + from, tlTextData(find));
    if (!p) return tlNull;
    return tlINT(p - tlTextData(text));
}

INTERNAL tlHandle _text_cat(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    tlText* add = tlTextCast(tlArgsGet(args, 0));
    if (!add) TL_THROW("arg must be a Text");

    return tlTextCat(text, add);
}

INTERNAL tlHandle _text_char(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");

    int at = tl_int_or(tlArgsGet(args, 0), 0);
    int size = tlTextSize(text);
    if (at < 0) at = size + at;
    if (!(at >=0 && at < size)) return tlNull;
    return tlINT(text->data[at]);
}

INTERNAL tlHandle _text_slice(tlArgs* args) {
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
    if (last >= size) last = size;

    trace("%d %d (%s)%d", first, last, text->data, size);

    return tlTextSub(text, first, last - first);
}

static inline char escape(char c) {
    switch (c) {
        case '\0': return '0';
        case '\t': return 't';
        case '\n': return 'n';
        case '\r': return 'r';
        case '"': return '"';
        case '$': return '$';
        case '\\': return '\\';
        default:
            //assert(c >= 32);
            return 0;
    }
}

INTERNAL tlHandle _text_escape(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");
    const char* data = tlTextData(text);
    int size = tlTextSize(text);
    int slashes = 0;
    for (int i = 0; i < size; i++) if (escape(data[i])) slashes++;
    if (slashes == 0) return text;

    char* ndata = malloc(size + slashes + 1);
    int j = 0;
    for (int i = 0; i < size; i++, j++) {
        char c;
        if ((c = escape(data[i]))) {
            ndata[j++] = '\\';
            ndata[j] = c;
        } else {
            ndata[j] = data[i];
        }
    }
    assert(size + slashes == j);
    return tlTextFromTake(ndata, j);
}

INTERNAL tlHandle _text_strip(tlArgs* args) {
    trace("");
    tlText* text = tlTextCast(tlArgsTarget(args));
    if (!text) TL_THROW("this must be a Text");

    if (tlTextSize(text) == 0) return text;
    const char* data = tlTextData(text);
    int len = tlTextSize(text);
    int left = 0;
    int right = len; // assuming zero byte at end

    while (left < right && data[left] <= 32) left++;
    while (right > left && data[right] <= 32) right--;
    right++; // always a zero byte

    trace("%d %d", left, right);
    if (left == len) return _tl_emptyText;
    if (left == 0 && right == len) return text;

    int size = right - left;
    assert(size > 0 && size < tlTextSize(text));
    char* ndata = malloc(size + 1);
    memcpy(ndata, data + left, size);
    ndata[size] = 0;
    return tlTextFromTake(ndata, size);
}

static int intmin(int left, int right) { return (left<right)?left:right; }

INTERNAL tlHandle _text_startsWith(tlArgs* args) {
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

INTERNAL tlHandle _text_endsWith(tlArgs* args) {
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

INTERNAL const char* textToText(tlHandle v, char* buf, int size) {
    return tlTextData(tlTextAs(v));
}
INTERNAL unsigned int textHash(tlHandle v) {
    return tlTextHash(tlTextAs(v));
}
INTERNAL int textEquals(tlHandle left, tlHandle right) {
    return tlTextEquals(tlTextAs(left), tlTextAs(right));
}
INTERNAL int textCmp(tlHandle left, tlHandle right) {
    return tlTextCmp(tlTextAs(left), tlTextAs(right));
}
static tlKind _tlTextKind = {
    .name = "text",
    .toText = textToText,
    .hash = textHash,
    .equals = textEquals,
    .cmp = textCmp,
};

static void text_init() {
    _tl_emptyText = tlTEXT("");
    _tlTextKind.klass = tlClassMapFrom(
        "toText", _text_toText,
        "toSym", _text_toSym,
        "size", _text_size,
        "hash", _text_hash,
        "startsWith", _text_startsWith,
        "endsWith", _text_endsWith,
        "find", _text_find,
        "char", _text_char,
        "get", _text_char,
        "slice", _text_slice,
        "cat", _text_cat,
        "escape", _text_escape,
        "strip", _text_strip,
        "each", null,
        "split", null,
        "replace", null,
        "eval", null,
        null
    );
    tlMap* constructor = tlClassMapFrom(
        "call", _Text_cat,
        "class", null,
        null
    );
    tlMapSetSym_(constructor, s_class, _tlTextKind.klass);
    tl_register_global("Text", constructor);
}

