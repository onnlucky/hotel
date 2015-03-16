// author: Onne Gorter, license: MIT (see license.txt)

#include "tests.h"

#include "platform.h"
#include "tl.h"

// TODO this should be 1G instead ;)
#define BASE 10
#define SCALE 10 // when doing div, how many digits to create after the dot by default
#define NEG -1
#define POS 1

struct tlNum {
    tlHead* head;
    int32_t sign;  // -1 or 1
    int32_t point; // where the floating point is, if larger than digits, it is prefixed zero's
    int32_t digits; // amount of data allocated
    int32_t data[]; // data[0] is the least significant digit
};

tlNum* tlNumNewInternal(int digits) {
    tlNum* d = calloc(1, sizeof(tlNum) + digits * sizeof(int32_t));
    d->digits = digits;
    d->sign = 1;
    return d;
}

tlNum* shrink(tlNum* d) {
    while (d->digits > 0 && !d->data[d->digits - 1]) d->digits -= 1;
    assert(d->sign == -1 || d->sign == 1);
    assert(d->digits  < 1000*1000*1000);
    assert(d->point < 1000*1000);
    assert(d->digits >= 0);
    return d;
}

// TODO base 16, etc
// TODO BASE 1G
tlNum* tlNumFromParse(const char* str, int base) {
    assert(base == 10);
    assert(BASE == 10);

    bool point = false;
    int digits = 0;
    for (int i = 0;; i++) {
        char c = str[i];
        if (!c) break;

        if (c >= '0' && c <= '9') {
            digits++;
        } else if (c == '.') {
            if (point) return null;
            point = true;
        } else if (c == '-') {
            if (digits != 0) return null;
        } else {
            return null;
        }
    }

    tlNum* res = tlNumNewInternal(digits);

    int i = 0;
    if (str[i] == '-') {
        res->sign = -1;
        i++;
    }

    int digit = 1;
    for (;; i++) {
        char c = str[i];
        if (!c) break;

        // TODO ensure to use BASE
        if (c >= '0' && c <= '9') {
            res->data[digits - digit] = c - '0';
            digit++;
        } else if (c == '.') {
            res->point = digits - digit + 1;
        }
    }

    return shrink(res);
}

tlNum* tlNumZero() {
    return tlNumNewInternal(0);
}

int tlNumToString(tlNum* d, char* buf, int len, int base) {
    assert(base == 10);
    assert(d->sign == -1 || d->sign == 1);

    len -= 1;
    int i = 0;
    if (d->sign == NEG && i < len) buf[i++] = '-';

    // if point is larger then size, we have prefixed zero's
    if (d->point > 0 && d->point >= d->digits) {
        if (i < len) buf[i++] = '0';
        if (i < len) buf[i++] = '.';
        for (int j = d->point; j > d->digits; j--) {
            if (i >= len) break;
            buf[i++] = '0';
        }
    }

    assert(BASE == 10); // TODO needs to change we we go larger
    for (int id = d->digits - 1; id >= 0; id--) {
        if (i >= len) break;
        buf[i++] = '0' + d->data[id];
        if (d->point > 0 && id == d->point) buf[i++] = '.';
    }
    buf[i] = 0;
    return i;
}

int tlNumGet(tlNum* d) {
    assert(d->point >= 0);
    assert(d->point < 1000 * 1000);
    int n = 0;
    for (int i = d->digits - 1; i >= d->point; i--) {
        assert(d->data[i] >= 0);
        assert(d->data[i] < BASE);
        n = n * BASE + d->data[i];
    }
    return d->sign * n;
}

tlNum* tlNumFromInt(int n, int point) {
    if (n == 0) return tlNumZero();

    int32_t sign = n < 0? -1 : 1;
    n *= sign;
    int digits = 1 + (int)log10(n);
    tlNum* d = tlNumNewInternal(digits);

    d->sign = sign;
    d->point = point;
    for (int i = 0; i < digits; i++) {
        d->data[i] = n % BASE;
        n /= BASE;
    }
    return d;
}

// TODO likely some optimizations here, like just checking if first digit is in same pos
int tlNumCompare(tlNum* lhs, tlNum* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int digits = max(lhs->digits, rhs->digits) + max(ldiff, rdiff);

    for (int i = digits - 1; i >= 0; i--) {
        int li = i - ldiff;
        int ri = i - rdiff;
        int l = (li >= 0 && li < lhs->digits)? lhs->data[li] : 0;
        int r = (ri >= 0 && ri < rhs->digits)? rhs->data[ri] : 0;

        if (l > r) return 1;
        if (r > l) return -1;
    }
    return 0;
}

tlNum* tlNumAdd2(tlNum* lhs, tlNum* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int digits = max(lhs->digits, rhs->digits) + max(ldiff, rdiff) + 1;

    tlNum* res = tlNumNewInternal(digits);

    int c = 0;
    for (int i = 0; i < digits; i++) {
        int li = i - ldiff;
        int ri = i - rdiff;
        int l = (li >= 0 && li < lhs->digits)? lhs->data[li] : 0;
        int r = (ri >= 0 && ri < rhs->digits)? rhs->data[ri] : 0;
        //print("[%d] = %d + %d + %d", i, l, r, c);
        res->data[i] = (l + r + c) % BASE;
        c = (l + r + c) / BASE;
    }

    res->sign = lhs->sign;
    res->point = point;
    return shrink(res);
}

tlNum* tlNumSub2(tlNum* lhs, tlNum* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int digits = max(lhs->digits, rhs->digits) + max(ldiff, rdiff);

    tlNum* res = tlNumNewInternal(digits);

    for (int i = 0; i < digits; i++) {
        int li = i - ldiff;
        int ri = i - rdiff;
        int l = (li >= 0 && li < lhs->digits)? lhs->data[li] : 0;
        int r = (ri >= 0 && ri < rhs->digits)? rhs->data[ri] : 0;
        int m = l - r;
        res->data[i] = m % BASE;
    }

    // TODO instead of all this, simply check if number is larger/smaller, and flip sign
    // TODO this can be folded into below at least, and likely into above ...
    int start = digits - 1;
    while (start >= 0 && res->data[start] == 0) start--;
    if (start >= 0 && res->data[start] < 0) {
        res->sign = -1;
        for (int i = 0; i <= start; i++) res->data[i] *= -1;
    } else {
        res->sign = 1;
    }
    assert(start == -1 || res->data[start] > 0);
    for (int i = 0; i < start; i++) {
        if (res->data[i] < 0) {
            res->data[i] = res->data[i] + 10;
            res->data[i + 1] -= 1;
        }
    }
    res->point = point;
    return shrink(res);
}

tlNum* tlNumAdd(tlNum* lhs, tlNum* rhs) {
    //  2 +  3 =          =  5
    // -2 +  3 =   3 - 2  =  1 // sub
    //  2 + -3 =   2 - 3  = -1 // sub
    // -2 + -3 = -(2 + 3) = -5
    if (lhs->sign != rhs->sign) return tlNumSub2(lhs, rhs);
    return tlNumAdd2(lhs, rhs);
}

tlNum* tlNumSub(tlNum* lhs, tlNum* rhs) {
    //  2 -  3 = -1
    // -2 -  3 = -5 // add
    //  2 - -3 =  5 // add
    // -2 - -3 =  1
    if (lhs->sign != rhs->sign) return tlNumAdd2(lhs, rhs);
    return tlNumSub2(lhs, rhs);
}

tlNum* tlNumMul(tlNum* lhs, tlNum* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int digits = lhs->digits + rhs->digits + max(ldiff, rdiff);
    tlNum* res = tlNumNewInternal(digits);

    for (int il = 0; il < lhs->digits + ldiff; il++) {
        int c = 0;
        for (int ir = 0; ir < rhs->digits + rdiff; ir++) {
            int li = il;// - ldiff;
            int ri = ir;// - rdiff;
            int l = (li >= 0 && li < lhs->digits)? lhs->data[li] : 0;
            int r = (ri >= 0 && ri < rhs->digits)? rhs->data[ri] : 0;
            int m = (res->data[il + ir] + l * r + c);
            //print("[%d] = %d (%d + %d * %d + %d", il + ir, m, res->data[il + ir], l, r, c);
            res->data[ir + il] = m % BASE;
            c = m / BASE;
        }
        res->data[rhs->digits + il] = res->data[rhs->digits + il] + c;
    }
    res->point = lhs->point + rhs->point;

    return shrink(res);
}

tlNum* tlNumDiv(tlNum* lhs, tlNum* rhs) {
    if (lhs->digits == 0) return tlNumZero(); // TODO preserve point/scale?
    assert(rhs->digits != 0); // TODO return inf/-inf etc. or NaN?

    int point = max(SCALE, max(lhs->point, rhs->point));
    assert(point >= 0);
    int digits = lhs->digits - lhs->point + point;
    assert(digits > 0);

    tlNum* row = tlNumNewInternal(digits);
    tlNum* res = tlNumNewInternal(digits);
    res->point = point;

    for (int i = 0; i < digits; i++) {
        // shift the row up
        for (int j = row->digits - 1; j > 0; j--) {
            row->data[j] = row->data[j - 1];
        }
        row->data[0] = i < lhs->digits? lhs->data[lhs->digits - i - 1] : 0;
        res->data[digits - i - 1] = 0;
        //print("[%d](%d) = %d", digits - i - 1, i, res->data[digits - i - 1]);
        while (tlNumCompare(row, rhs) >= 0) {
            res->data[digits - i - 1]++;
            //print("[%d](%d) = %d", digits - i - 1, i, res->data[digits - i - 1]);
            // TODO inplace sub implementation?
            tlNum* tmp = tlNumSub(row, rhs);
            for (int j = 0; j < row->digits; j++) {
                row->data[j] = j < tmp->digits? tmp->data[j] : 0;
            }
        }
    }

    // move point to cut of any trailing zeros, unless they were there to begin with
    int move = 0;
    int maxmove = res->point - max(lhs->point, rhs->point);
    while (move < maxmove && res->data[move] == 0) move++;
    if (move > 0) {
        for (int i = move; i < res->digits; i++) {
            res->data[i - move] = res->data[i];
            res->data[i] = 0;
        }
        res->point -= move;
    }

    return shrink(res);
}

// super high powers are likely too big
tlNum* tlNumPow(tlNum* lhs, int rhs) {
    if (rhs <= 0) {
        tlNum* res = lhs;
        for (int i = 0; i >= rhs; i--) {
            res = tlNumDiv(res, lhs);
        }
        return res;
    }

    tlNum* res = lhs;
    for (int i = 2; i <= rhs; i++) {
        res = tlNumMul(res, lhs);
    }
    return res;
}

TEST(num_new) {
    char buf[1024];

    {
        const char* in = "1.00000001";
        tlNum* n = tlNumFromParse(in, 10);
        tlNumToString(n, buf, sizeof(buf), 10);
        print("RES: %s", buf);
        REQUIRE(!strcmp(buf, in));
    }

    {
        const char* in = "639484762348172678767542998373847465109285546378276244155263401928400345308701304010506057464.1092874554938374612039363548112300999094463726627399226789";
        tlNum* n = tlNumFromParse(in, 10);
        REQUIRE(n);
        tlNumToString(n, buf, sizeof(buf), 10);
        print("RES: %s", buf);
        REQUIRE(!strcmp(buf, in));
    }
}

TEST(build_and_print) {
    char buf[1024];
    tlNum* d;

    d = tlNumFromInt(450, 0);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "450"));

    d = tlNumFromInt(450, 1);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "45.0"));

    d = tlNumFromInt(450, 2);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "4.50"));

    d = tlNumFromInt(450, 3);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.450"));

    d = tlNumFromInt(450, 4);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.0450"));

    d = tlNumFromInt(-4501, 1);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "-450.1"));

    d = tlNumFromInt(-4501, 10);
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "-0.0000004501"));

    d = tlNumFromInt(450, 4);
    d = tlNumAdd(d, tlNumFromInt(56111, 3));
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "56.1560"));

    d = tlNumFromInt(450, 4);
    d = tlNumSub(d, tlNumFromInt(56111, 3));
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "-56.0660"));
}

TEST(basic) {
    char buf[1024];
    REQUIRE(tlNumGet(tlNumFromInt(0, 0)) == 0);
    REQUIRE(tlNumGet(tlNumFromInt(9, 0)) == 9);
    REQUIRE(tlNumGet(tlNumFromInt(18, 0)) == 18);
    REQUIRE(tlNumGet(tlNumFromInt(27, 0)) == 27);
    REQUIRE(tlNumGet(tlNumFromInt(4876927, 0)) == 4876927);

    for (int i = 0; i < 150; i += 7) {
        print("%d", i);

        tlNum* n1 = tlNumFromInt(50, 0);
        tlNum* n2 = tlNumFromInt(i, 0);

        tlNum* r1 = tlNumSub(n1, n2);
        tlNumToString(r1, buf, sizeof(buf), 10);
        //print("50 - %d = %s", i, buf);
        REQUIRE(tlNumGet(r1) == 50 - i);
        tlNum* r2 = tlNumSub(n2, n1);
        tlNumToString(r2, buf, sizeof(buf), 10);
        //print("%d - 50 = %s", i, buf);
        REQUIRE(tlNumGet(r2) == i - 50);

        tlNum* r3 = tlNumAdd(n1, n2);
        REQUIRE(tlNumGet(r3) == 50 + i);

        tlNum* r4 = tlNumMul(n1, n2);
        REQUIRE(tlNumGet(r4) == 50 * i);

        if (i != 0) {
            tlNum* r5 = tlNumDiv(n1, n2);
            //print("50 / %d = %d (%d)", i, tlNumGet(r5), 50 / i);
            REQUIRE(tlNumGet(r5) == 50 / i);
        }
        tlNum* r6 = tlNumDiv(n2, n1);
        REQUIRE(tlNumGet(r6) == i / 50);
    }

    tlNum* n1 = tlNumFromInt(109, 0);
    tlNum* n2 = tlNumFromInt(274, 0);
    tlNum* r;

    r = tlNumAdd(n1, n2);
    REQUIRE(tlNumGet(r) == 109 + 274);

    r = tlNumSub(n1, n2);
    REQUIRE(tlNumGet(r) == 109 - 274);

    r = tlNumSub(n2, n1);
    REQUIRE(tlNumGet(r) == 274 - 109);

    r = tlNumMul(n1, n2);
    REQUIRE(tlNumGet(r) == 109 * 274);
    REQUIRE(tlNumGet(tlNumMul(tlNumFromInt(19, 0), tlNumFromInt(378, 0))) == 19 * 378);

    REQUIRE(tlNumGet(tlNumDiv(tlNumFromInt(1032, 0), tlNumFromInt(3, 0))) == 1032 / 3);
    REQUIRE(tlNumGet(tlNumDiv(tlNumFromInt(1032, 0), tlNumFromInt(31, 0))) == 1032 / 31);

    r = tlNumDiv(n1, n2);
    REQUIRE(tlNumGet(r) == 109 / 274);

    r = tlNumDiv(n2, n1);
    REQUIRE(tlNumGet(r) == 274 / 109);
    REQUIRE(tlNumGet(tlNumDiv(tlNumFromInt(19, 0), tlNumFromInt(378, 0))) == 19 / 378);
    REQUIRE(tlNumGet(tlNumDiv(tlNumFromInt(378, 0), tlNumFromInt(19, 0))) == 378 / 19);
}

TEST(mul) {
    char buf[1024];
    tlNum* d = tlNumMul(tlNumFromInt(101, 0), tlNumFromInt(1, 5));
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.00101"));

    d = tlNumMul(tlNumFromInt(101, 2), tlNumFromInt(33, 5));
    tlNumToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.0003333"));
}

TEST(pow) {
    char buf[1024];

    {
        tlNum* n = tlNumPow(tlNumFromParse("2", 10), 32);
        tlNumToString(n, buf, sizeof(buf), 10);
        print("RES: %s", buf);
        REQUIRE(!strcmp(buf, "4294967296"));
    }

    {
        tlNum* n = tlNumPow(tlNumFromParse("2", 10), -4);
        tlNumToString(n, buf, sizeof(buf), 10);
        print("RES: %s", buf);
        REQUIRE(!strcmp(buf, "0.0625"));
    }
}

int main(int argc, char** argv) {
    tl_init();
    RUN(num_new);
    RUN(build_and_print);
    RUN(basic);
    RUN(mul);
    RUN(pow);
}

