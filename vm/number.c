// author: Onne Gorter, license: MIT (see license.txt)

// most of the int/float/char/bignum implementations are here, for now bignum is libtommath based

// TODO move *all* int and float related stuff here
// TODO replace libtommath for a bigdecimal implementation, see number_test.c, slower, but 0.1 + 0.2 == 0.3

#include <tommath.h>

#include "platform.h"
#include "number.h"

#include "value.h"
#include "string.h"
#include "buffer.h"

// ** int **

static const char* inttoString(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "%zd", tlIntToInt(v)); return buf;
}
static unsigned int intHash(tlHandle v, tlHandle* unhashable) {
    intptr_t i = tlIntToInt(v);
    return murmurhash2a(&i, sizeof(i));
}
static bool intEquals(tlHandle left, tlHandle right) {
    return left == right;
}
static tlHandle intCmp(tlHandle left, tlHandle right) {
    intptr_t l = tlIntToInt(left);
    intptr_t r = tlIntToInt(right);
    if (l < r) return tlSmaller;
    if (l > r) return tlLarger;
    return tlEqual;
}
static tlKind _tlIntKind = {
        .name = "Int",
        .index = -3,
        .toString = inttoString,
        .hash = intHash,
        .equals = intEquals,
        .cmp = intCmp,
};

static tlHandle _int_hash(tlTask* task, tlArgs* args) {
    return tlINT(intHash(tlArgsTarget(args), null));
}

static tlHandle _int_toChar(tlTask* task, tlArgs* args) {
    intptr_t c = tlIntToInt(tlArgsTarget(args));
    if (c < 0) c = 32;
    if (c > 255) c = 32;
    if (c < 0) TL_THROW("negative numbers cannot be a char");
    if (c > 255) TL_THROW("utf8 not yet supported");
    const char buf[] = { c, 0 };
    return tlStringFromCopy(buf, 1);
}

static tlHandle _int_toString(tlTask* task, tlArgs* args) {
    intptr_t c = tlIntToInt(tlArgsTarget(args));
    int base = tl_int_or(tlArgsGet(args, 0), 10);
    char buf[128];
    int len = 0;
    switch (base) {
        case 2:
        {
            // TODO 7 == 0111; -7 = 1001; ~7 = 11111111111111111111111111111000
            int at = 0;
            bool zero = true;
            for (int i = 31; i >= 0; i--) {
                if ((c >> i) & 1) {
                    zero = false;
                    buf[at++] = '1';
                } else {
                    if (zero) continue;
                    buf[at++] = '0';
                }
                len = at;
                buf[len] = 0;
            }
        }
            break;
        case 10:
            len = snprintf(buf, sizeof(buf), "%zd", c);
            break;
        case 16:
            len = snprintf(buf, sizeof(buf), "%zx", c);
            break;
        default:
            TL_THROW("base must be 10 (default), 2 or 16");
    }
    return tlStringFromCopy(buf, len);
}

// TODO upto 8 bytes, this shouldn't be architecture dependend
static tlHandle _int_bytes(tlTask* task, tlArgs* args) {
    intptr_t n = tlIntToInt(tlArgsTarget(args));
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++) bytes[3 - i] = (uint8_t)(n >> (8 * i));
    return tlBinFromCopy((const char*)bytes, 4);
}

static tlHandle _int_self(tlTask* task, tlArgs* args) {
    return tlArgsTarget(args);
}

static tlHandle _int_abs(tlTask* task, tlArgs* args) {
    intptr_t i = tlIntToInt(tlArgsTarget(args));
    if (i < 0) return tlINT(-i);
    return tlArgsTarget(args);
}

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
void write_utf8(int c, char buf[], int* len) {
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
        write_utf8(0xFFFD, buf, len);
    }
}

static const char* chartoString(tlHandle v, char* buf, int size) {
    int len = 0;
    write_utf8(tl_int(v), buf, &len);
    buf[len] = 0;
    return buf;
}
static uint32_t charHash(tlHandle h, tlHandle* unhashable) {
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
    return tlINT(charHash(tlArgsTarget(args), null));
}
static tlHandle _char_toString(tlTask* task, tlArgs* args) {
    int c = tl_int(tlArgsTarget(args));
    char buf[8];
    int len = 0;
    write_utf8(c, buf, &len);
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
static uint32_t floatHash(tlHandle h, tlHandle* unhashable) {
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
    return tlINT(floatHash(tlArgsTarget(args), null));
}
static tlHandle _float_bytes(tlTask* task, tlArgs* args) {
    union { uint64_t n; double f; } u;
    u.f = tl_double(tlArgsTarget(args));
    char bytes[8];
    for (int i = 0; i < 8; i++) bytes[7 - i] = u.n >> (8 * i);
    return tlBinFromCopy(bytes, 8);
}
static tlHandle _float_abs(tlTask* task, tlArgs* args) {
    return tlFLOAT(fabs(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_floor(tlTask* task, tlArgs* args) {
    return tlNumberFromDouble(floor(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_round(tlTask* task, tlArgs* args) {
    return tlNumberFromDouble(round(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_ceil(tlTask* task, tlArgs* args) {
    return tlNumberFromDouble(ceil(tl_double(tlArgsTarget(args))));
}
static tlHandle _float_toString(tlTask* task, tlArgs* args) {
    double c = tl_double(tlArgsTarget(args));
    int base = 10;
    char buf[255];
    int len = 0;
    if (tlArgsSize(args) == 1) {
        if (!tlNumberIs(tlArgsGet(args, 0))) TL_THROW("Float.toString(base=10) base must be a number");
        base = tl_int(tlArgsGet(args, 0));
    }

    if (base == 10) {
        len = snprintf(buf, sizeof(buf), "%.17g", c);
    } else if (base == 16) {
        len = snprintf(buf, sizeof(buf), "%a", c);
    } else {
        TL_THROW("Float.toString() does not support a base of %d", base);
    }
    return tlStringFromCopy(buf, len);
}

// ** bignum **

struct tlNum {
    tlHead* head;
    mp_int value;
};

void numFinalizer(tlHandle handle) {
    tlNum* num = tlNumAs(handle);
    mp_clear(&num->value);
}

// TODO implement from double ...
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

tlHandle tlNumberFromDouble(double d) {
    if (d >= TL_MIN_INT && d <= TL_MAX_INT) return tlINT((intptr_t)d);
    warning("might be an incorrect number conversion: %f == %lld", d, (long long)d);
    return tlNumNew((int64_t)d);
}

tlNum* tlNumTo(tlHandle h) {
    assert(tlNumberIs(h));
    if (tlNumIs(h)) return tlNumAs(h);
    return tlNumNew(tl_int(h));
}

intptr_t tlNumToInt(tlNum* num) {
    return mp_get_signed(&num->value);
}
double tlNumToDouble(tlNum* num) {
    return (double)mp_get_int(&num->value);
}
tlNum* tlNumNeg(tlNum* num) {
    mp_int res; mp_init(&res);
    mp_neg(&num->value, &res);
    return tlNumberFrom(res);
}
tlNum* tlNumAdd(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_add(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
tlNum* tlNumSub(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_sub(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
tlNum* tlNumMul(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_mul(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
tlNum* tlNumDiv(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_int remainder; mp_init(&remainder);
    mp_div(&l->value, &r->value, &res, &remainder);
    mp_clear(&remainder);
    return tlNumberFrom(res);
}
tlNum* tlNumRem(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_int remainder; mp_init(&remainder);
    mp_div(&l->value, &r->value, &res, &remainder);
    mp_clear(&res);
    return tlNumberFrom(remainder);
}
tlNum* tlNumMod(tlNum* l, tlNum* r) {
    mp_int res; mp_init(&res);
    mp_mod(&l->value, &r->value, &res);
    return tlNumberFrom(res);
}
tlNum* tlNumPow(tlNum* l, intptr_t r) {
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
static uint32_t numHash(tlHandle h, tlHandle* unhashable) {
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
        else mp_set_signed(&nleft, tlIntToInt(left));
        cmp = mp_cmp(&nleft, &tlNumAs(right)->value);
    } else if (!tlNumIs(right)) {
        mp_int nright; mp_init(&nright);
        if (tlFloatIs(right)) mp_set_double(&nright, tl_double(right));
        else mp_set_signed(&nright, tlIntToInt(right));
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
    .finalizer = numFinalizer,
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

int64_t tlNumberToInt64(tlHandle h) {
    if (tlIntIs(h)) return tlIntToInt(h);
    if (tlFloatIs(h)) return (int64_t)tlFloatAs(h)->value;
    if (tlNumIs(h)) return tlNumToInt(tlNumAs(h));
    if (tlCharIs(h)) return tlCharAs(h)->value;
    return INT64_MIN;
}

int tl_int(tlHandle h) {
    int64_t i;
    if (tlIntIs(h)) i = tlIntToInt(h);
    else if (tlFloatIs(h)) i = (int64_t)tlFloatAs(h)->value;
    else if (tlNumIs(h)) i = tlNumToInt(tlNumAs(h));
    else if (tlCharIs(h)) i = tlCharAs(h)->value;
    else { assert(false); return INT_MIN; }

    //if (!(i > INT_MIN && i < INT_MAX)) fatal("bad number: %s %lld", tl_str(h), i);
    if (i < INT_MIN) return INT_MIN;
    if (i > INT_MAX) return INT_MAX;
    return (int)i;
}

int tl_int_or(tlHandle h, int d) {
    int64_t i;
    if (tlIntIs(h)) i = tlIntToInt(h);
    else if (tlFloatIs(h)) i = (int64_t)tlFloatAs(h)->value;
    else if (tlNumIs(h)) i = tlNumToInt(tlNumAs(h));
    else if (tlCharIs(h)) i = tlCharAs(h)->value;
    else return d;

    //if (!(i > INT_MIN && i < INT_MAX)) fatal("bad number: %s %lld", tl_str(h), i);
    if (i < INT_MIN) return INT_MIN;
    if (i > INT_MAX) return INT_MAX;
    return (int)i;
}

double tl_double(tlHandle h) {
    if (tlNullIs(h)) return 0;
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

//// math operators

static tlHandle _number_add(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        return tlNUM(tlIntToInt(lhs) + tlIntToInt(rhs));
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if (tlFloatIs(lhs) || tlFloatIs(rhs)) {
        return tlFLOAT(tl_double(lhs) + tl_double(rhs));
    } else {
        return tlNumAdd(tlNumTo(lhs), tlNumTo(rhs));
    }
}

static tlHandle _number_sub(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        return tlNUM(tlIntToInt(lhs) - tlIntToInt(rhs));
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if (tlFloatIs(lhs) || tlFloatIs(rhs)) {
        return tlFLOAT(tl_double(lhs) - tl_double(rhs));
    } else {
        return tlNumSub(tlNumTo(lhs), tlNumTo(rhs));
    }
}

#define TL_MIN_HALF (TL_MIN_INT >> (sizeof(intptr_t) * 8 / 2))
#define TL_MAX_HALF (TL_MAX_INT >> (sizeof(intptr_t) * 8 / 2))

static tlHandle _number_mul(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        intptr_t l = tlIntToInt(lhs);
        intptr_t r = tlIntToInt(rhs);

        // check for overflow
        if (l > TL_MIN_HALF && l < TL_MAX_HALF && r > TL_MIN_HALF && r < TL_MAX_HALF) {
            return tlINT(l * r);
        }
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if (tlFloatIs(lhs) || tlFloatIs(rhs)) {
        return tlFLOAT(tl_double(lhs) * tl_double(rhs));
    } else {
        return tlNumMul(tlNumTo(lhs), tlNumTo(rhs));
    }
}

// TODO should return bigdecimal's instead
static tlHandle _number_div(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        return tlFLOAT(tlIntToInt(lhs) / (double)tlIntToInt(rhs));
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if (tlFloatIs(lhs) || tlFloatIs(rhs)) {
        return tlFLOAT(tl_double(lhs) / tl_double(rhs));
    } else {
        return tlFLOAT(tl_double(lhs) / tl_double(rhs));
        // TODO when tlNum is bigdecimal
        //return tlNumDiv(tlNumTo(lhs), tlNumTo(rhs));
    }
}

static tlHandle _number_idiv(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        return tlINT(tlIntToInt(lhs) / tlIntToInt(rhs));
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    // TODO when tlNum is bigdecimal, must round
    return tlNumDiv(tlNumTo(lhs), tlNumTo(rhs));
}

static tlHandle _number_fdiv(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        return tlFLOAT(tlIntToInt(lhs) / (double)tlIntToInt(rhs));
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    return tlFLOAT(tl_double(lhs) / tl_double(rhs));
}

static tlHandle _number_rem(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        return tlNUM(tlIntToInt(lhs) % tlIntToInt(rhs));
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if (tlFloatIs(lhs) || tlFloatIs(rhs)) {
        return tlFLOAT(fmod(tl_double(lhs), tl_double(rhs)));
    } else {
        return tlNumRem(tlNumTo(lhs), tlNumTo(rhs));
    }
}

static tlHandle _number_mod(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (tlIntIs(lhs) && tlIntIs(rhs)) {
        intptr_t res = tlIntToInt(lhs) % tlIntToInt(rhs);
        return tlINT(res > 0? res : -res);
    }

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if (tlFloatIs(lhs) || tlFloatIs(rhs)) {
        return tlFLOAT(fabs(fmod(tl_double(lhs), tl_double(rhs))));
    } else {
        return tlNumMod(tlNumTo(lhs), tlNumTo(rhs));
    }
}

static tlHandle _number_pow(tlTask* task, tlArgs* args) {
    tlHandle lhs = tlArgsLhs(args);
    tlHandle rhs = tlArgsRhs(args);

    if (!tlNumberIs(lhs) || !tlNumberIs(rhs)) return tlUndef();

    if ((tlFloatIs(lhs) || tlFloatIs(rhs))) {
        return tlFLOAT(pow(tl_double(lhs), tl_double(rhs)));
    }

    // TODO check if really small? or will that work out as zero anyhow?
    int p = tl_int(rhs);
    if (p < 0) return tlFLOAT(pow(tl_double(lhs), p));
    if (p < 1000) return tlNumPow(tlNumTo(lhs), p);
    TL_THROW("'**' out of range: %s ** %d", tl_str(lhs), p);
}

// TODO shifting should work on arbitrary size number include tlNUM
static tlHandle _number_lshift(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsLhs(args);
    tlHandle r = tlArgsRhs(args);

    if (!tlNumberIs(l) || !tlNumberIs(r)) return tlUndef();

    int64_t lhs = tlNumberToInt64(l);
    int rhs = tl_int(r);
    if (rhs <= -64 || rhs >= 64) return tlZero;
    uint64_t res = rhs >= 0? lhs << rhs : lhs >> -rhs;
    return tlNUM((intptr_t)res);
}

static tlHandle _number_rshift(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsLhs(args);
    tlHandle r = tlArgsRhs(args);

    if (!tlNumberIs(l) || !tlNumberIs(r)) return tlUndef();

    int64_t lhs = tlNumberToInt64(l);
    int rhs = tl_int(r);
    if (rhs <= -64 || rhs >= 64) return tlZero;
    uint64_t res = rhs >= 0? lhs >> rhs : lhs << -rhs;
    return tlNUM((intptr_t)res);
}

static tlHandle _number_rrshift(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsLhs(args);
    tlHandle r = tlArgsRhs(args);

    if (!tlNumberIs(l) || !tlNumberIs(r)) return tlUndef();

    uint32_t lhs = tlNumberToInt64(l);
    int rhs = tl_int(r);
    if (rhs < -64 || rhs > 64) return tlZero;
    uint32_t res = rhs >= 0? lhs >> rhs : lhs << -rhs;
    return tlNUM((uint32_t)res);
}

static tlHandle number_cmp(tlHandle lhs, tlHandle rhs) {
    if (tlIntIs(lhs) && tlIntIs(rhs)) return intCmp(lhs, rhs);
    if (tlNumIs(lhs) || tlNumIs(rhs)) return numCmp(lhs, rhs);
    if (tlFloatIs(lhs) || tlFloatIs(rhs)) return floatCmp(lhs, rhs);
    return tlUndef();
}

static tlHandle _number_lt(tlTask* task, tlArgs* args) {
    tlHandle res = number_cmp(tlArgsLhs(args), tlArgsRhs(args));
    if (res == tlSmaller) return tlTrue;
    if (res == tlEqual || res == tlLarger) return tlFalse;
    return res;
}

static tlHandle _number_lte(tlTask* task, tlArgs* args) {
    tlHandle res = number_cmp(tlArgsLhs(args), tlArgsRhs(args));
    if (res == tlSmaller || tlEqual) return tlTrue;
    if (res == tlLarger) return tlFalse;
    return res;
}

static tlHandle _number_gt(tlTask* task, tlArgs* args) {
    tlHandle res = number_cmp(tlArgsLhs(args), tlArgsRhs(args));
    if (res == tlSmaller || tlEqual) return tlFalse;
    if (res == tlLarger) return tlTrue;
    return res;
}

static tlHandle _number_gte(tlTask* task, tlArgs* args) {
    tlHandle res = number_cmp(tlArgsLhs(args), tlArgsRhs(args));
    if (res == tlSmaller) return tlFalse;
    if (res == tlEqual || res == tlLarger) return tlTrue;
    return res;
}

void number_init() {
    mp_init(&MIN_INT_BIGNUM);
    mp_init(&MAX_INT_BIGNUM);

    // mp_get_signed gives 62 or 31 bits, should work exactly ...
    mp_set_signed(&MIN_INT_BIGNUM, TL_MIN_INT);
    mp_set_signed(&MAX_INT_BIGNUM, TL_MAX_INT);

    assert(tl_int(tlINT(100)) == 100);
    assert(tl_int(tlINT(-100)) == -100);
    assert(tl_int(tlINT(123456)) == 123456);
    assert(tl_int(tlINT(-123456)) == -123456);
    assert(tlINT(0) == tlZero);
    assert(tlINT(1) == tlOne);
    assert(tlINT(2) == tlTwo);

    assert(tlINT(TL_MAX_INT) == tlIntMax);
    assert(tlINT(TL_MIN_INT) == tlIntMin);
    assert(tlIntToInt(tlIntMax) == TL_MAX_INT);
    assert(tlIntToInt(tlIntMin) == TL_MIN_INT);

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

    _tlIntKind.klass = tlClassObjectFrom(
        "hash", _int_hash,
        "bytes", _int_bytes,
        "abs", _int_abs,
        "toChar", _int_toChar,
        "toString", _int_toString,
        "floor", _int_self,
        "round", _int_self,
        "ceil", _int_self,
        "times", null,
        "to", null,

        "+", _number_add,
        "-", _number_sub,
        "*", _number_mul,
        "/", _number_div,
        "//", _number_idiv,
        "/.", _number_fdiv,
        "%", _number_mod,
        "**", _number_pow,

        "<<", _number_lshift,
        ">>", _number_rshift,
        ">>>", _number_rrshift,

        "<", _number_lt,
        "<=", _number_lte,
        ">", _number_gt,
        ">=", _number_gte,
        null
    );
    tlObject* intStatic = tlClassObjectFrom(
            "_methods", null,
            null
    );
    tlObjectSet_(intStatic, s__methods, _tlIntKind.klass);
    tl_register_global("Int", intStatic);
    INIT_KIND(tlIntKind);

    _tlFloatKind.klass = tlClassObjectFrom(
        "hash", _float_hash,
        "bytes", _float_bytes,
        "abs", _float_abs,
        "floor", _float_floor,
        "round", _float_round,
        "ceil", _float_ceil,
        "toString", _float_toString,

        "+", _number_add,
        "-", _number_sub,
        "*", _number_mul,
        "/", _number_div,
        "//", _number_idiv,
        "/.", _number_fdiv,
        "%", _number_mod,
        "**", _number_pow,

        "<<", _number_lshift,
        ">>", _number_rshift,
        ">>>", _number_rrshift,

        "<", _number_lt,
        "<=", _number_lte,
        ">", _number_gt,
        ">=", _number_gte,
        null
    );

    _tlNumKind.klass = tlClassObjectFrom(
        "toString", _num_toString,
        "abs", _num_abs,
        "bytes", _num_bytes,

        "+", _number_add,
        "-", _number_sub,
        "*", _number_mul,
        "/", _number_div,
        "//", _number_idiv,
        "/.", _number_fdiv,
        "%", _number_mod,
        "**", _number_pow,


        "<<", _number_lshift,
        ">>", _number_rshift,
        ">>>", _number_rrshift,

        "<", _number_lt,
        "<=", _number_lte,
        ">", _number_gt,
        ">=", _number_gte,
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

