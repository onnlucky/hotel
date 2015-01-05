// author: Onne Gorter, license: MIT (see license.txt)
// simple immutable binary buffer type

#include "trace-off.h"

tlKind* tlBinKind;

static tlBin* _tl_emptyBin;

struct tlBin {
    tlHead head;
    bool interned; // same as string; TODO remove, but for binEquals
    unsigned int hash;
    unsigned int len;
    const char* data;
};

tlBin* tlBinEmpty() { return _tl_emptyBin; }

tlBin* tlBinNew(int len) {
    assert(len >= 0);
    tlBin* bin = tlAlloc(tlBinKind, sizeof(tlBin));
    char* data = malloc_atomic(len + 1);
    memset(data, 0, len + 1);
    bin->len = len;
    bin->data = data;
    return bin;
}

tlBin* tlBinFromCopy(const char* s, int len) {
    trace("%s", s);
    assert(len >= 0);

    char* data = malloc_atomic(len + 1);
    memcpy(data, s, len);
    data[len] = 0;
    tlBin* bin = tlAlloc(tlBinKind, sizeof(tlBin));
    bin->len = len;
    bin->data = data;
    return bin;
}

tlBin* tlBinFromTake(const char* s, int len) {
    assert(len >= 0);
    tlBin* bin = tlAlloc(tlBinKind, sizeof(tlBin));
    bin->len = len;
    bin->data = s;
    return bin;
}

tlBin* tlBinFromBufferTake(tlBuffer* buf) {
    int len = tlBufferSize(buf);
    const char* s = tlBufferTakeData(buf);
    return tlBinFromTake(s, len);
}

const char* tlBinData(tlBin *bin) {
    assert(tlBinIs(bin));
    return bin->data;
}

int tlBinSize(tlBin* bin) {
    assert(tlBinIs(bin));
    return bin->len;
}
int tlBinGet(tlBin* bin, int at) {
     if (at < 0 || at >= tlBinSize(bin)) return -1;
     return bin->data[at] & 0xFF;
}

void tlBinSet_(tlBin* bin, int at, char byte) {
    assert(at >= 0 && at < bin->len);
    assert(bin->data[at] == 0);
    ((char*)bin->data)[at] = byte;
}

tlBin* tlBinFrom2(tlHandle b1, tlHandle b2) {
    tlBuffer* buf = tlBufferNew();
    const char* error = null;
    buffer_write_object(buf, b1, &error);
    assert(!error);
    buffer_write_object(buf, b2, &error);
    assert(!error);
    return tlBinFromCopy(tlBufferData(buf), tlBufferSize(buf));
}

tlBin* tlBinCat(tlBin* b1, tlBin* b2) {
    int b1s = tlBinSize(b1);
    if (b1s == 0) return b2;
    int b2s = tlBinSize(b2);
    if (b2s == 0) return b1;

    char* data = malloc_atomic(b1s + b2s);
    memcpy(data, tlBinData(b1), b1s);
    memcpy(data + b1s, tlBinData(b2), b2s);
    tlBin* bin = tlAlloc(tlBinKind, sizeof(tlBin));
    bin->len = b1s + b2s;
    bin->data = data;
    return bin;
}

tlBin* tlBinSub(tlBin* from, int offset, int len) {
    if (tlBinSize(from) == len) return from;
    if (len == 0) return tlBinEmpty();

    assert(len > 0);
    assert(offset >= 0);
    assert(offset + len <= tlBinSize(from));

    char* data = malloc_atomic(len + 1);
    memcpy(data, from->data + offset, len);
    data[len] = 0;

    return tlBinFromTake(data, len);
}

INTERNAL tlHandle _Bin_new(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferNew();

    const char* error = null;
    for (int i = 0;; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        buffer_write_object(buf, v, &error);
        if (error) TL_THROW("%s", error);
    }
    return tlBinFromCopy(tlBufferData(buf), tlBufferSize(buf));
}

INTERNAL tlHandle _bin_size(tlTask* task, tlArgs* args) {
    return tlINT(tlBinAs(tlArgsTarget(args))->len);
}
INTERNAL tlHandle _bin_get(tlTask* task, tlArgs* args) {
    tlBin* bin = tlBinAs(tlArgsTarget(args));
    int at = at_offset(tlArgsGet(args, 0), tlBinSize(bin));
    if (at < 0) return tlUndef();
    return tlINT(tlBinGet(bin, at));
}
/// slice: return a subsection of the binary withing bounds of args[1], args[2]
INTERNAL tlHandle _bin_slice(tlTask* task, tlArgs* args) {
    tlBin* str = tlBinAs(tlArgsTarget(args));
    int first = tl_int_or(tlArgsGet(args, 0), 1);
    int last = tl_int_or(tlArgsGet(args, 1), -1);

    int offset;
    int len = sub_offset(first, last, tlBinSize(str), &offset);
    return tlBinSub(str, offset, len);
}
INTERNAL tlHandle _bin_toHex(tlTask* task, tlArgs* args) {
    static const char hexchars[] =
        {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    tlBin* bin = tlBinAs(tlArgsTarget(args));
    int len = bin->len * 2;
    char* buf = malloc_atomic(len + 1);
    for (int i = 0; i < bin->len; i++) {
        uint8_t c = bin->data[i];
        buf[i * 2 + 0] = hexchars[c >> 4 & 0x0F];
        buf[i * 2 + 1] = hexchars[c >> 0 & 0x0F];
    }
    buf[len] = 0;
    return tlStringFromTake(buf, len);
}

static tlHandle _bin_toString(tlTask* task, tlArgs* args) {
    tlBin* bin = tlBinAs(tlArgsTarget(args));

    char* into = malloc_atomic(bin->len + 1);
    int written = 0; int chars = 0;
    int read = process_utf8(bin->data, bin->len, &into, &written, &chars);
    assert(chars >= written / 5);
    UNUSED(read);
    // TODO undefined char? warn? throw? configurable?

    return tlStringFromTake(into, written);
}

INTERNAL const char* bintoString(tlHandle v, char* buf, int size) {
    tlBin* bin = tlBinAs(v);
    if (tlBinSize(bin) == 0) {
        snprintf(buf, size, "Bin()");
        return buf;
    }
    size -= snprintf(buf, size, "Bin(");
    int i = 0;
    for (; i < tlBinSize(bin) && size > 6; i++) {
        size -= snprintf(buf + 4 + 5 * i, size, "0x%02X,", tlBinGet(bin, i));
    }
    if (size > 6) snprintf(buf + 4 + 5 * i - 1, 2, ")");
    else snprintf(buf + 4 + 5 * i - 5, 5, "...)");
    return buf;
}
INTERNAL unsigned int binHash(tlHandle v) {
    return tlStringHash((tlString*)v);
}
INTERNAL int binEquals(tlHandle left, tlHandle right) {
    return tlStringEquals((tlString*)left, (tlString*)right);
}
INTERNAL tlHandle binCmp(tlHandle left, tlHandle right) {
    return tlCOMPARE(tlStringCmp((tlString*)left, (tlString*)right));
}
static tlKind _tlBinKind = {
    .name = "bin",
    .toString = bintoString,
    .hash = binHash,
    .equals = binEquals,
    .cmp = binCmp,
};

static void bin_init() {
    INIT_KIND(tlBinKind);

    char buf[0];
    _tl_emptyBin = tlBinFromCopy(buf, 0);

    tlBinKind->klass = tlClassObjectFrom(
        "toString", _bin_toString,
        "toHex", _bin_toHex,
        //"toBuffer", _bin_toBuffer,
        "size", _bin_size,
        //"hash", _bin_hash,
        "get", _bin_get,
        "slice", _bin_slice,
        "find", null,
        "each", null,
        "map", null,
        "random", null,
        null
    );
    tlObject* constructor = tlClassObjectFrom(
        "call", _Bin_new,
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s__methods, tlBinKind->klass);
    tl_register_global("Bin", constructor);
}

