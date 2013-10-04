#include <tommath.h>

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
    snprintf(buf, size, "%f", tl_double(v)); return buf;
}
static unsigned int floatHash(tlHandle h) {
    return murmurhash2a((uint8_t*)&tlFloatAs(h)->value, sizeof(double));
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
    return tlUndef();
}

static tlKind _tlFloatKind = {
    .name = "Float",
    .toString = floattoString,
    .hash = floatHash,
    .equals = floatEquals,
    .cmp = floatCmp,
};
tlKind* tlFloatKind = &_tlFloatKind;

static tlHandle _float_hash(tlArgs* args) {
    return tlINT(floatHash(tlArgsTarget(args)));
}
static tlHandle _float_floor(tlArgs* args) {
    return tlINT(floor(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_round(tlArgs* args) {
    return tlINT(round(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_ceil(tlArgs* args) {
    return tlINT(ceil(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_toString(tlArgs* args) {
    double c = tl_double(tlArgsTarget(args));
    char buf[255];
    int len = snprintf(buf, sizeof(buf), "%f", c);
    return tlStringFromCopy(buf, len);
}

// ** bignum **

struct tlNum {
    tlHead* head;
    mp_int value;
};

tlNum* tlNumNew(intptr_t l) {
    tlNum* num = tlAlloc(tlNumKind, sizeof(tlNum));
    mp_init(&num->value);
    mp_set_signed(&num->value, l);
    return num;
}
tlNum* tlNumFrom(mp_int n) {
    tlNum* num = tlAlloc(tlNumKind, sizeof(tlNum));
    num->value = n;
    return num;
}
intptr_t tlNumToInt(tlNum* num) {
    return mp_get_int(&num->value);
}
intptr_t tlNumToDouble(tlNum* num) {
    return (double)mp_get_int(&num->value);
}

static const char* numtoString(tlHandle v, char* buf, int size) {
    mp_toradix_n(&tlNumAs(v)->value, buf, 10, size); return buf;
}
static unsigned int numHash(tlHandle h) {
    return murmurhash2a((uint8_t*)&tlNumAs(h)->value, sizeof(double));
}
static bool numEquals(tlHandle left, tlHandle right) {
    return mp_cmp(&tlNumAs(left)->value, &tlNumAs(right)->value) == MP_EQ;
}
static tlHandle numCmp(tlHandle left, tlHandle right) {
    int cmp = mp_cmp(&tlNumAs(left)->value, &tlNumAs(right)->value);
    if (cmp < 0) return tlSmaller;
    if (cmp > 0) return tlLarger;
    return tlEqual;
}

static tlKind _tlNumKind = {
    .name = "Number",
    .toString = numtoString,
    .hash = numHash,
    .equals = numEquals,
    .cmp = numCmp,
};
tlKind* tlNumKind = &_tlNumKind;


// ** various conversions **
static mp_int MIN_INT_BIGNUM;
static mp_int MAX_INT_BIGNUM;

tlHandle tlPARSENUM(const char* s, int radix) {
    mp_int n;
    mp_init(&n);
    mp_read_radix(&n, s, radix);
    if (mp_cmp(&n, &MIN_INT_BIGNUM) >= 0 && mp_cmp(&n, &MAX_INT_BIGNUM) <= 0) {
        return tlINT(mp_get_signed(&n));
    }
    warning("BIGNUM: %s", s);
    return tlNumFrom(n);
}

tlHandle tlNUM(intptr_t l) {
    if (l >= TL_MIN_INT && l <= TL_MAX_INT) return tlINT(l);
    return tlNumNew(l);
}

intptr_t tl_int(tlHandle h) {
    if (tlIntIs(h)) return tlIntToInt(h);
    if (tlFloatIs(h)) return (intptr_t)tlFloatAs(h)->value;
    if (tlNumIs(h)) return tlNumToInt(tlNumAs(h));
    assert(false);
    return 0;
}
intptr_t tl_int_or(tlHandle h, int d) {
    if (tlIntIs(h)) return tlIntToInt(h);
    if (tlFloatIs(h)) return (intptr_t)tlFloatAs(h)->value;
    if (tlNumIs(h)) return tlNumToInt(tlNumAs(h));
    return d;
}
double tl_double(tlHandle h) {
    if (tlFloatIs(h)) return tlFloatAs(h)->value;
    if (tlIntIs(h)) return tlIntToDouble(h);
    if (tlNumIs(h)) return tlNumToDouble(tlNumAs(h));
    assert(false);
    return NAN;
}
double tl_double_or(tlHandle h, double d) {
    if (tlFloatIs(h)) return tlFloatAs(h)->value;
    if (tlIntIs(h)) return tlIntToDouble(h);
    if (tlNumIs(h)) return tlNumToDouble(tlNumAs(h));
    return d;
}

static void number_init() {
    mp_init(&MIN_INT_BIGNUM);
    mp_init(&MAX_INT_BIGNUM);

    // mp_get_signed gives 62 or 31 bits, should work exactly ...
    mp_set_signed(&MIN_INT_BIGNUM, TL_MIN_INT);
    mp_set_signed(&MAX_INT_BIGNUM, TL_MAX_INT);

    mp_int ZERO; mp_init(&ZERO); mp_set(&ZERO, 0);
    assert(mp_cmp(&MIN_INT_BIGNUM, &ZERO) == -1);

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

    _tlFloatKind.klass = tlClassMapFrom(
        "hash", _float_hash,
        "floor", _float_floor,
        "round", _float_round,
        "ceil", _float_ceil,
        "toString", _float_toString,
        null
    );

    //_tlNumKind.klass = tlClassMapFrom(
    //    "toString", _num_toString,
    //    null
    //);
}

