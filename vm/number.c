#include <tommath.h>

// ** char **

struct tlChar {
    tlHead* head;
    int value;
};

tlHandle tlCHAR(int value) {
    tlChar* c = tlAlloc(tlCharKind, sizeof(tlChar));
    c->value = value;
    return c;
}

// error char: 0xFFFD
// max char: 0x10FFFF
void charToUtf8(int c, char buf[], int* len) {
    if (c < 0x80) { // 7 bits
        buf[0] = c;
        *len = 1;
    } else if (c < 0x800) { // 11 bits
        buf[0] = 0xC0 | (c >> 6);
        buf[1] = 0x80 | (c & 0x3F);
        *len = 2;
    } else if (c < 0x10000) { // 16 bits
        buf[0] = 0xE0 | (c >> 12);
        buf[1] = 0x80 | ((c >> 6) & 0x3F);
        buf[2] = 0x80 | (c & 0x3F);
        *len = 3;
    } else if (c < 0x200000) { // 21 bits
        buf[0] = 0xF0 | (c >> 18);
        buf[1] = 0x80 | ((c >> 12) & 0x3F);
        buf[2] = 0x80 | ((c >> 6) & 0x3F);
        buf[3] = 0x80 | (c & 0x3F);
        *len = 4;
    } else {
        // encode the error char
        charToUtf8(0xFFFD, buf, len);
    }
}

static const char* chartoString(tlHandle v, char* buf, int size) {
    int len = 0;
    charToUtf8(tl_int(v), buf, &len);
    buf[len] = 0;
    return buf;
}
static uint32_t charHash(tlHandle h) {
    return murmurhash2a((uint8_t*)&tlCharAs(h)->value, sizeof(int)) + 2;
}
static bool charEquals(tlHandle left, tlHandle right) {
    return tl_double(left) == tl_double(right);
}
static tlHandle charCmp(tlHandle left, tlHandle right) {
    double l = tl_double(left);
    double r = tl_double(right);
    if (l > r) return tlLarger;
    if (l < r) return tlSmaller;
    if (l == r) return tlEqual;
    return tlUndef();
}

static tlKind _tlCharKind = {
    .name = "Char",
    .index = -6,
    .toString = chartoString,
    .hash = charHash,
    .equals = charEquals,
    .cmp = charCmp,
};
tlKind* tlCharKind;

static tlHandle _Char(tlTask* task, tlArgs* args) {
    // TODO take from bin/buffer/string other chars and numbers
    // TODO verify it is unicode code point
    return tlCHAR(tl_int(tlArgsGet(args, 0)));
}
static tlHandle _char_hash(tlTask* task, tlArgs* args) {
    return tlINT(charHash(tlArgsTarget(args)));
}
static tlHandle _char_toString(tlTask* task, tlArgs* args) {
    int c = tl_int(tlArgsTarget(args));
    char buf[8];
    int len = 0;
    charToUtf8(c, buf, &len);
    buf[len] = 0;
    return tlStringFromCopy(buf, len);
}
static tlHandle _char_toNumber(tlTask* task, tlArgs* args) {
    return tlINT(tl_int(tlArgsTarget(args)));
}

// ** float **

struct tlFloat {
    tlHead* head;
    double value;
};

tlHandle tlFLOAT(double d) {
    tlFloat* f = tlAlloc(tlFloatKind, sizeof(tlFloat));
    f->value = d;
    return f;
}

static const char* floattoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "%.17g", tl_double(v)); return buf;
}
static uint32_t floatHash(tlHandle h) {
    return murmurhash2a((uint8_t*)&tlFloatAs(h)->value, sizeof(double)) + 3;
}
static bool floatEquals(tlHandle left, tlHandle right) {
    return tl_double(left) == tl_double(right);
}
static tlHandle floatCmp(tlHandle left, tlHandle right) {
    double l = tl_double(left);
    double r = tl_double(right);
    if (l > r) return tlLarger;
    if (l < r) return tlSmaller;
    if (l == r) return tlEqual;
    // TODO this is almost impossible ... but NaN <> NaN causes this
    return null;
}

static tlKind _tlFloatKind = {
    .name = "Float",
    .index = -4,
    .toString = floattoString,
    .hash = floatHash,
    .equals = floatEquals,
    .cmp = floatCmp,
};
tlKind* tlFloatKind;

static tlHandle _float_hash(tlTask* task, tlArgs* args) {
    return tlINT(floatHash(tlArgsTarget(args)));
}
static tlHandle _float_bytes(tlTask* task, tlArgs* args) {
    double d = tl_double(tlArgsTarget(args));
    uint64_t n = *(uint64_t*)&d;
    assert(d == *(double*)&n);
    char bytes[8];
    for (int i = 0; i < 8; i++) bytes[7 - i] = n >> (8 * i);
    return tlBinFromCopy(bytes, 8);
}
static tlHandle _float_abs(tlTask* task, tlArgs* args) {
    return tlFLOAT(abs(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_floor(tlTask* task, tlArgs* args) {
    return tlNUM(floor(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_round(tlTask* task, tlArgs* args) {
    return tlNUM(round(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_ceil(tlTask* task, tlArgs* args) {
    return tlNUM(ceil(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_toString(tlTask* task, tlArgs* args) {
    double c = tl_double(tlArgsTarget(args));
    char buf[255];
    int len = snprintf(buf, sizeof(buf), "%.17g", c);
    return tlStringFromCopy(buf, len);
}

// ** bignum **

struct tlNum {
    tlHead* head;
    mp_int value;
};
void clear_num(void* _num, void* data) {
    tlNum* num = tlNumAs(_num);
    mp_clear(&num->value);
}
// TODO implement from double ...
tlNum* tlNumNew(intptr_t l) {
    tlNum* num = tlAlloc(tlNumKind, sizeof(tlNum));
    mp_init(&num->value);
    mp_set_signed(&num->value, l);
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(num, clear_num, null, null, null);
#endif
    return num;
}
tlNum* tlNumFrom(mp_int n) {
    tlNum* num = tlAlloc(tlNumKind, sizeof(tlNum));
    num->value = n;
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(num, clear_num, null, null, null);
#endif
    return num;
}

// return a int or bignum depending on how many bits the representation needs
static mp_int MIN_INT_BIGNUM;
static mp_int MAX_INT_BIGNUM;
tlHandle tlNumberFrom(mp_int n) {
    if (mp_cmp(&n, &MIN_INT_BIGNUM) >= 0 && mp_cmp(&n, &MAX_INT_BIGNUM) <= 0) {
        tlHandle res = tlINT(mp_get_signed(&n));
        mp_clear(&n);
        return res;
    }
    return tlNumFrom(n);
}

tlNum* tlNumTo(tlHandle h) {
    assert(tlNumberIs(h));
    if (tlNumIs(h)) return tlNumAs(h);
    return tlNumNew(tl_int(h));
}

intptr_t tlNumToInt(tlNum* num) {
    return mp_get_int(&num->value);
}
double tlNumToDouble(tlNum* num) {
    return (double)mp_get_int(&num->value);
}
static tlNum* tlNumNeg(tlNum* num) {
    mp_int res; mp_init(&res);
    mp_neg(&num->value, &res);
    return tlNumberFrom(res);
}
static tlNum* tlNumAdd(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_add(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
static tlNum* tlNumSub(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_sub(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
static tlNum* tlNumMul(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_mul(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
static tlNum* tlNumDiv(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_int remainder; mp_init(&remainder);
    mp_div(&l->value, &r->value, &res, &remainder);
    return tlNumberFrom(res);
}
static tlNum* tlNumMod(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_mod(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
static tlNum* tlNumPow(tlNum* l, intptr_t r) {
    mp_int res; mp_init(&res);
    mp_expt_d(&l->value, r, &res);
    return tlNumberFrom(res);
}

static tlHandle _num_abs(tlTask* task, tlArgs* args) {
    tlNum* num = tlNumAs(tlArgsTarget(args));
    return num;
}
static tlHandle _num_toString(tlTask* task, tlArgs* args) {
    tlNum* num = tlNumAs(tlArgsTarget(args));
    int base = tl_int_or(tlArgsGet(args, 0), 10);
    char buf[1024];
    switch (base) {
        case 2:
        mp_toradix_n(&num->value, buf, 2, sizeof(buf));
        break;
        case 10:
        mp_toradix_n(&num->value, buf, 10, sizeof(buf));
        break;
        case 16:
        mp_toradix_n(&num->value, buf, 16, sizeof(buf));
        break;
        default:
        TL_THROW("base must be 10 (default), 2 or 16");
    }
    return tlStringFromCopy(buf, 0);
}

static tlHandle _num_bytes(tlTask* task, tlArgs* args) {
    tlNum* num = tlNumAs(tlArgsTarget(args));

    // count how many bytes
    mp_int t;
    mp_digit d;
    mp_init_copy(&t, &num->value);

    int bytes = 1; // first bit is sign
    mp_div_d(&t, (mp_digit)128, &t, &d);

    while (mp_iszero(&t) == MP_NO) {
        bytes += 1;
        mp_div_d(&t, (mp_digit)256, &t, &d);
    }
    mp_clear(&t);

    // now output them into a tlBin
    tlBin* bin = tlBinNew(bytes);

    mp_init_copy(&t, &num->value);
    mp_div_d(&t, 128, &t, &d);
    assert(d <= 127);
    tlBinSet_(bin, 0, d | t.sign << 7);

    bytes = 0;
    while (mp_iszero(&t) == MP_NO) {
        bytes += 1;
        mp_div_d(&t, 256, &t, &d);
        tlBinSet_(bin, bytes, d);
    }

    mp_clear(&t);
    return bin;
}

tlNum* tlNumFromBin(tlBin* bin) {
    mp_int t;
    mp_init(&t);
    mp_set(&t, 0);

    for (int i = tlBinSize(bin) - 1; i >= 1; i--) {
        mp_mul_2d(&t, 8, &t);
        mp_add_d(&t, (mp_digit)tlBinGet(bin, i), &t);
    }
    mp_mul_2d(&t, 7, &t);
    mp_add_d(&t, (mp_digit)(tlBinGet(bin, 0) & 0x7F), &t);
    t.sign = (tlBinGet(bin, 0) & 0x80) == 0x80? MP_NEG : MP_ZPOS;

    return tlNumFrom(t);
}

tlNum* tlNumFromBuffer(tlBuffer* buf, int len) {
    assert(tlBufferSize(buf) >= len);
    mp_int t;
    mp_init(&t);
    mp_set(&t, 0);

    for (int i = len - 1; i >= 1; i--) {
        mp_mul_2d(&t, 8, &t);
        mp_add_d(&t, (mp_digit)tlBufferPeekByte(buf, i), &t);
    }

    int last = tlBufferPeekByte(buf, 0);
    mp_mul_2d(&t, 7, &t);
    mp_add_d(&t, (mp_digit)(last & 0x7F), &t);
    t.sign = (last & 0x80) == 0x80? MP_NEG : MP_ZPOS;

    tlBufferReadSkip(buf, len);
    return tlNumFrom(t);
}

static tlHandle _Num_fromBytes(tlTask* task, tlArgs* args) {
    tlBin* bin = tlBinCast(tlArgsGet(args, 0));
    if (!bin) TL_THROW("expected a Bin as args[1]");
    return tlNumFromBin(bin);
}

static const char* numtoString(tlHandle v, char* buf, int size) {
    mp_toradix_n(&tlNumAs(v)->value, buf, 10, size); return buf;
}
static uint32_t numHash(tlHandle h) {
    // TODO this is broken
    return murmurhash2a((uint8_t*)&tlNumAs(h)->value, sizeof(double));
}
static bool numEquals(tlHandle left, tlHandle right) {
    return mp_cmp(&tlNumAs(left)->value, &tlNumAs(right)->value) == MP_EQ;
}
static tlHandle numCmp(tlHandle left, tlHandle right) {
    int cmp;
    if (!tlNumIs(left)) {
        mp_int nleft; mp_init(&nleft);
        if (tlFloatIs(left)) mp_set_double(&nleft, tl_double(left));
        else mp_set_signed(&nleft, tl_int(left));
        cmp = mp_cmp(&nleft, &tlNumAs(right)->value);
    } else if (!tlNumIs(right)) {
        mp_int nright; mp_init(&nright);
        if (tlFloatIs(right)) mp_set_double(&nright, tl_double(right));
        else mp_set_signed(&nright, tl_int(right));
        cmp = mp_cmp(&tlNumAs(left)->value, &nright);
    } else {
        cmp = mp_cmp(&tlNumAs(left)->value, &tlNumAs(right)->value);
    }
    if (cmp < 0) return tlSmaller;
    if (cmp > 0) return tlLarger;
    return tlEqual;
}

static tlKind _tlNumKind = {
    .name = "Number",
    .index = -5,
    .toString = numtoString,
    .hash = numHash,
    .equals = numEquals,
    .cmp = numCmp,
};
tlKind* tlNumKind;


// ** various conversions **
tlHandle tlPARSENUM(const char* s, int radix) {
    mp_int n;
    mp_init(&n);
    mp_read_radix(&n, s, radix);
    return tlNumberFrom(n);
}

tlHandle tlNUM(intptr_t l) {
    if (l >= TL_MIN_INT && l <= TL_MAX_INT) return tlINT(l);
    return tlNumNew(l);
}

intptr_t tl_int(tlHandle h) {
    if (tlIntIs(h)) return tlIntToInt(h);
    if (tlFloatIs(h)) return (intptr_t)tlFloatAs(h)->value;
    if (tlNumIs(h)) return tlNumToInt(tlNumAs(h));
    if (tlCharIs(h)) return tlCharAs(h)->value;
    assert(false);
    return 0;
}
intptr_t tl_int_or(tlHandle h, int d) {
    if (tlIntIs(h)) return tlIntToInt(h);
    if (tlFloatIs(h)) return (intptr_t)tlFloatAs(h)->value;
    if (tlNumIs(h)) return tlNumToInt(tlNumAs(h));
    if (tlCharIs(h)) return tlCharAs(h)->value;
    return d;
}
double tl_double(tlHandle h) {
    if (tlFloatIs(h)) return tlFloatAs(h)->value;
    if (tlIntIs(h)) return tlIntToDouble(h);
    if (tlNumIs(h)) return tlNumToDouble(tlNumAs(h));
    if (tlCharIs(h)) return tlCharAs(h)->value;
    assert(false);
    return NAN;
}
double tl_double_or(tlHandle h, double d) {
    if (tlFloatIs(h)) return tlFloatAs(h)->value;
    if (tlIntIs(h)) return tlIntToDouble(h);
    if (tlNumIs(h)) return tlNumToDouble(tlNumAs(h));
    if (tlCharIs(h)) return tlCharAs(h)->value;
    return d;
}

static void number_init() {
    mp_init(&MIN_INT_BIGNUM);
    mp_init(&MAX_INT_BIGNUM);

    // mp_get_signed gives 62 or 31 bits, should work exactly ...
    mp_set_signed(&MIN_INT_BIGNUM, TL_MIN_INT);
    mp_set_signed(&MAX_INT_BIGNUM, TL_MAX_INT);

    assert(tlINT(TL_MAX_INT) == tlIntMax);
    assert(tlINT(TL_MIN_INT) == tlIntMin);
    assert(tlINT(0) == tlZero);
    assert(tlINT(1) == tlOne);
    assert(tlINT(2) == tlTwo);

    mp_int ZERO; mp_init(&ZERO); mp_set(&ZERO, 0);
    assert(mp_cmp(&MIN_INT_BIGNUM, &ZERO) == -1);
    mp_clear(&ZERO);

    mp_int test; mp_init(&test);
    mp_set_signed(&test, 1);
    assert(1 == mp_get_signed(&test));
    mp_set_signed(&test, -1);
    assert(-1 == mp_get_signed(&test));
    mp_set_signed(&test, -2000);
    assert(-2000 == mp_get_signed(&test));

    mp_set_signed(&test, TL_MAX_INT);
    assert(TL_MAX_INT == mp_get_signed(&test));

    mp_set_signed(&test, TL_MIN_INT);
    assert(TL_MIN_INT == mp_get_signed(&test));

    mp_set_double(&test, 0.1);
    assert(0 == mp_get_signed(&test));
    mp_set_double(&test, -0.1);
    assert(0 == mp_get_signed(&test));

    mp_set_double(&test, 1.1);
    assert(1 == mp_get_signed(&test));
    mp_set_double(&test, -1.1);
    assert(-1 == mp_get_signed(&test));

    mp_set_double(&test, 1.9);
    assert(2 == mp_get_signed(&test));
    mp_set_double(&test, 5);
    assert(5 == mp_get_signed(&test));

    mp_set_double(&test, 5000000.752341);
    assert(5000001 == mp_get_signed(&test));
    mp_set_double(&test, -5000000.752341);
    assert(-5000001 == mp_get_signed(&test));

    mp_set_double(&test, 1e100);
    assert(mp_cmp(&MAX_INT_BIGNUM, &test) == -1);
    // TODO assert ("%.0f", 1e100) == mp_to_char(test);

    mp_clear(&test);

    _tlFloatKind.klass = tlClassObjectFrom(
        "hash", _float_hash,
        "bytes", _float_bytes,
        "abs", _float_abs,
        "floor", _float_floor,
        "round", _float_round,
        "ceil", _float_ceil,
        "toString", _float_toString,
        null
    );

    _tlNumKind.klass = tlClassObjectFrom(
        "toString", _num_toString,
        "abs", _num_abs,
        "bytes", _num_bytes,
        null
    );
    tlObject* nconstructor = tlClassObjectFrom(
        "fromBytes", _Num_fromBytes,
        "_methods", null,
        null
    );
    tlObjectSet_(nconstructor, s__methods, _tlNumKind.klass);
    tl_register_global("Num", nconstructor);

    _tlCharKind.klass = tlClassObjectFrom(
        "hash", _char_hash,
        "toString", _char_toString,
        "toNumber", _char_toNumber,
        "toChar", _char_toString,
        null
    );
    tlObject* cconstructor = tlClassObjectFrom(
        "call", _Char,
        "_methods", null,
        null
    );
    tlObjectSet_(cconstructor, s__methods, _tlCharKind.klass);
    tl_register_global("Char", cconstructor);

    INIT_KIND(tlFloatKind);
    INIT_KIND(tlNumKind);
    INIT_KIND(tlCharKind);
}

