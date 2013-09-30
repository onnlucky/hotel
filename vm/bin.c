// author: Onne Gorter, license: MIT (see license.txt)
// simple immutable binary buffer type

#include "trace-off.h"

static tlKind _tlBinKind;
tlKind* tlBinKind = &_tlBinKind;

static tlBin* _tl_emptyBin;

struct tlBin {
    tlHead head;
    unsigned int hash;
    unsigned int len;
    const char* data;
};

tlBin* tlBinEmpty() { return _tl_emptyBin; }

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

const uint8_t* tlBinData(tlBin *bin) {
    assert(tlBinIs(bin));
    return (uint8_t*)bin->data;
}

int tlBinSize(tlBin* bin) {
    assert(tlBinIs(bin));
    return bin->len;
}

INTERNAL tlHandle _Bin_new(tlArgs* args) {
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

INTERNAL tlHandle _bin_size(tlArgs* args) {
    return tlINT(tlBinAs(tlArgsTarget(args))->len);
}
INTERNAL tlHandle _bin_get(tlArgs* args) {
    tlBin* bin = tlBinAs(tlArgsTarget(args));
    int at = tl_int_or(tlArgsGet(args, 0), -1);
    if (at < 1 || at > bin->len) TL_THROW("out of bounds");
    return tlINT(bin->data[at - 1]);
}
INTERNAL tlHandle _bin_toString(tlArgs* args) {
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
    .hash = binHash,
    .equals = binEquals,
    .cmp = binCmp,
};

static void bin_init() {
    char buf[0];
    _tl_emptyBin = tlBinFromCopy(buf, 0);

    _tlBinKind.klass = tlClassMapFrom(
        "toString", _bin_toString,
        //"toBuffer", _bin_toBuffer,
        "size", _bin_size,
        //"hash", _bin_hash,
        "get", _bin_get,
        //"slice", _bin_slice,
        null
    );
    tlMap* constructor = tlClassMapFrom(
        "call", _Bin_new,
        "class", null,
        null
    );
    tlMapSetSym_(constructor, s_class, _tlBinKind.klass);
    tl_register_global("bin", constructor);
}

