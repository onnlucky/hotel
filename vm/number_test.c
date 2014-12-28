#include "tests.h"

#include "tl.h"

// TODO this should be 1G instead ;)
#define BASE 10
#define NEG -1
#define POS 1

typedef struct tlDecimal {
    tlHead* head;
    int32_t sign;  // -1 or 1
    uint32_t point; // where the floating point is, if larger than size, it is prefixed zero's
    uint32_t size; // amount of data allocated
    int32_t data[]; // data[0] is the least significant digit
} tlDecimal;

tlDecimal* tlDecimalNewInternal(int size) {
    tlDecimal* d = calloc(1, sizeof(tlDecimal) + size * sizeof(int32_t));
    d->size = size;
    d->sign = 1;
    return d;
}

tlDecimal* tlDecimalZero() {
    return tlDecimalNewInternal(0);
}

tlDecimal* shrink(tlDecimal* d) {
    while (d->size > 0 && !d->data[d->size - 1]) d->size -= 1;
    assert(d->sign == -1 || d->sign == 1);
    assert(d->size  < 1000*1000*1000);
    assert(d->point < 1000*1000);
    assert(d->size >= 0);
    return d;
}

int tlDecimalToString(tlDecimal* d, char* buf, int len, int base) {
    assert(base == 10);
    assert(d->sign == -1 || d->sign == 1);

    len -= 1;
    int i = 0;
    if (d->sign == NEG && i < len) buf[i++] = '-';

    // if point is larger then size, we have prefixed zero's
    if (d->point > 0 && d->point >= d->size) {
        if (i < len) buf[i++] = '0';
        if (i < len) buf[i++] = '.';
        for (int j = d->point; j > d->size; j--) {
            if (i >= len) break;
            buf[i++] = '0';
        }
    }

    assert(BASE == 10); // TODO needs to change we we go larger
    for (int id = d->size - 1; id >= 0; id--) {
        if (i >= len) break;
        buf[i++] = '0' + d->data[id];
        if (d->point > 0 && id == d->point) buf[i++] = '.';
    }
    buf[i] = 0;
    return i;
}

int tlDecimalToInt(tlDecimal* d) {
    int n = 0;
    for (int i = d->size - 1; i >= 0; i--) {
        assert(d->data[i] >= 0);
        assert(d->data[i] < BASE);
        n = n * BASE + d->data[i];
    }
    return d->sign * n;
}

tlDecimal* tlDecimalFrom(int n, int point) {
    if (n == 0) return tlDecimalZero();

    int32_t sign = n < 0? -1 : 1;
    n *= sign;
    int size = 1 + (int)log10(n);
    tlDecimal* d = tlDecimalNewInternal(size);

    d->sign = sign;
    d->point = point;
    for (int i = 0; i < size; i++) {
        d->data[i] = n % BASE;
        n /= BASE;
    }
    return d;
}

tlDecimal* tlDecimalFromInt(int n) {
    return tlDecimalFrom(n, 0);
}

// TODO likely some optimizations here, like just checking if first digit is in same pos
int tlDecimalCompare(tlDecimal* lhs, tlDecimal* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int size = max(lhs->size, rhs->size) + max(ldiff, rdiff);

    for (int i = size - 1; i >= 0; i--) {
        int li = i - ldiff;
        int ri = i - rdiff;
        int l = (li >= 0 && li < lhs->size)? lhs->data[li] : 0;
        int r = (ri >= 0 && ri < rhs->size)? rhs->data[ri] : 0;

        if (l > r) return 1;
        if (r > l) return -1;
    }
    return 0;
}

tlDecimal* tlDecimalAdd2(tlDecimal* lhs, tlDecimal* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int size = max(lhs->size, rhs->size) + max(ldiff, rdiff) + 1;

    tlDecimal* res = tlDecimalNewInternal(size);

    int c = 0;
    for (int i = 0; i < size; i++) {
        int li = i - ldiff;
        int ri = i - rdiff;
        int l = (li >= 0 && li < lhs->size)? lhs->data[li] : 0;
        int r = (ri >= 0 && ri < rhs->size)? rhs->data[ri] : 0;
        //print("[%d] = %d + %d + %d", i, l, r, c);
        res->data[i] = (l + r + c) % BASE;
        c = (l + r + c) / BASE;
    }

    res->sign = lhs->sign;
    res->point = point;
    return shrink(res);
}

tlDecimal* tlDecimalSub2(tlDecimal* lhs, tlDecimal* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int size = max(lhs->size, rhs->size) + max(ldiff, rdiff);

    tlDecimal* res = tlDecimalNewInternal(size);

    for (int i = 0; i < size; i++) {
        int li = i - ldiff;
        int ri = i - rdiff;
        int l = (li >= 0 && li < lhs->size)? lhs->data[li] : 0;
        int r = (ri >= 0 && ri < rhs->size)? rhs->data[ri] : 0;
        int m = l - r;
        res->data[i] = m % BASE;
    }

    // TODO instead of all this, simply check if number is larger/smaller, and flip sign
    // TODO this can be folded into below at least, and likely into above ...
    int start = size - 1;
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

tlDecimal* tlDecimalAdd(tlDecimal* lhs, tlDecimal* rhs) {
    //  2 +  3 =          =  5
    // -2 +  3 =   3 - 2  =  1 // sub
    //  2 + -3 =   2 - 3  = -1 // sub
    // -2 + -3 = -(2 + 3) = -5
    if (lhs->sign != rhs->sign) return tlDecimalSub2(lhs, rhs);
    return tlDecimalAdd2(lhs, rhs);
}

tlDecimal* tlDecimalSub(tlDecimal* lhs, tlDecimal* rhs) {
    //  2 -  3 = -1
    // -2 -  3 = -5 // add
    //  2 - -3 =  5 // add
    // -2 - -3 =  1
    if (lhs->sign != rhs->sign) return tlDecimalAdd2(lhs, rhs);
    return tlDecimalSub2(lhs, rhs);
}

tlDecimal* tlDecimalMul(tlDecimal* lhs, tlDecimal* rhs) {
    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int size = lhs->size + rhs->size + max(ldiff, rdiff);
    tlDecimal* res = tlDecimalNewInternal(size);

    for (int il = 0; il < lhs->size + ldiff; il++) {
        int c = 0;
        for (int ir = 0; ir < rhs->size + rdiff; ir++) {
            int li = il;// - ldiff;
            int ri = ir;// - rdiff;
            int l = (li >= 0 && li < lhs->size)? lhs->data[li] : 0;
            int r = (ri >= 0 && ri < rhs->size)? rhs->data[ri] : 0;
            int m = (res->data[il + ir] + l * r + c);
            //print("[%d] = %d (%d + %d * %d + %d", il + ir, m, res->data[il + ir], l, r, c);
            res->data[ir + il] = m % BASE;
            c = m / BASE;
        }
        res->data[rhs->size + il] = res->data[rhs->size + il] + c;
    }
    res->point = lhs->point + rhs->point;

    return shrink(res);
}

tlDecimal* tlDecimalDiv(tlDecimal* lhs, tlDecimal* rhs) {
    if (lhs->size == 0) return tlDecimalZero(); // TODO preserve point?
    assert(rhs->size != 0); // TODO return inf/-inf etc. or NaN?

    int point = max(lhs->point, rhs->point);
    int ldiff = point - lhs->point;
    int rdiff = point - rhs->point;
    assert(point >= 0 && ldiff >= 0 && rdiff >= 0);
    int size = lhs->size + max(ldiff, rdiff);
    assert(size > 0);

    tlDecimal* row = tlDecimalNewInternal(size);
    row->point = point;
    tlDecimal* res = tlDecimalNewInternal(size);
    row->point = point;

    for (int il = lhs->size - 1; il >= 0; il--) {
        // shift the row up
        for (int i = row->size - 1; i > 0; i--) {
            row->data[i] = row->data[i - 1];
        }
        row->data[0] = lhs->data[il];
        res->data[il] = 0;
        while (tlDecimalCompare(row, rhs) >= 0) {
            res->data[il]++;
            //print("[%d] = %d", il, res->data[il]);
            // TODO inplace sub implementation?
            tlDecimal* tmp = tlDecimalSub(row, rhs);
            for (int i = 0; i < row->size; i++) {
                row->data[i] = i < tmp->size? tmp->data[i] : 0;
            }
        }
    }

    return shrink(res);
}

TEST(build_and_print) {
    char buf[1024];
    tlDecimal* d;

    d = tlDecimalFrom(450, 0);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "450"));

    d = tlDecimalFrom(450, 1);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "45.0"));

    d = tlDecimalFrom(450, 2);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "4.50"));

    d = tlDecimalFrom(450, 3);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.450"));

    d = tlDecimalFrom(450, 4);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.0450"));

    d = tlDecimalFrom(-4501, 1);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "-450.1"));

    d = tlDecimalFrom(-4501, 10);
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "-0.0000004501"));

    d = tlDecimalFrom(450, 4);
    d = tlDecimalAdd(d, tlDecimalFrom(56111, 3));
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "56.1560"));

    d = tlDecimalFrom(450, 4);
    d = tlDecimalSub(d, tlDecimalFrom(56111, 3));
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "-56.0660"));
}

TEST(basic) {
    char buf[1024];
    REQUIRE(tlDecimalToInt(tlDecimalFromInt(0)) == 0);
    REQUIRE(tlDecimalToInt(tlDecimalFromInt(9)) == 9);
    REQUIRE(tlDecimalToInt(tlDecimalFromInt(18)) == 18);
    REQUIRE(tlDecimalToInt(tlDecimalFromInt(27)) == 27);
    REQUIRE(tlDecimalToInt(tlDecimalFromInt(4876927)) == 4876927);

    for (int i = 0; i < 150; i += 7) {
        print("%d", i);

        tlDecimal* n1 = tlDecimalFromInt(50);
        tlDecimal* n2 = tlDecimalFromInt(i);

        tlDecimal* r1 = tlDecimalSub(n1, n2);
        tlDecimalToString(r1, buf, sizeof(buf), 10);
        //print("50 - %d = %s", i, buf);
        REQUIRE(tlDecimalToInt(r1) == 50 - i);
        tlDecimal* r2 = tlDecimalSub(n2, n1);
        tlDecimalToString(r2, buf, sizeof(buf), 10);
        //print("%d - 50 = %s", i, buf);
        REQUIRE(tlDecimalToInt(r2) == i - 50);

        tlDecimal* r3 = tlDecimalAdd(n1, n2);
        REQUIRE(tlDecimalToInt(r3) == 50 + i);

        tlDecimal* r4 = tlDecimalMul(n1, n2);
        REQUIRE(tlDecimalToInt(r4) == 50 * i);

        if (i != 0) {
            tlDecimal* r5 = tlDecimalDiv(n1, n2);
            REQUIRE(tlDecimalToInt(r5) == 50 / i);
        }
        tlDecimal* r6 = tlDecimalDiv(n2, n1);
        REQUIRE(tlDecimalToInt(r6) == i / 50);
    }

    tlDecimal* n1 = tlDecimalFromInt(109);
    tlDecimal* n2 = tlDecimalFromInt(274);
    tlDecimal* r;

    r = tlDecimalAdd(n1, n2);
    REQUIRE(tlDecimalToInt(r) == 109 + 274);

    r = tlDecimalSub(n1, n2);
    REQUIRE(tlDecimalToInt(r) == 109 - 274);

    r = tlDecimalSub(n2, n1);
    REQUIRE(tlDecimalToInt(r) == 274 - 109);

    r = tlDecimalMul(n1, n2);
    REQUIRE(tlDecimalToInt(r) == 109 * 274);
    REQUIRE(tlDecimalToInt(tlDecimalMul(tlDecimalFromInt(19), tlDecimalFromInt(378))) == 19 * 378);

    REQUIRE(tlDecimalToInt(tlDecimalDiv(tlDecimalFromInt(1032), tlDecimalFromInt(3))) == 1032 / 3);
    REQUIRE(tlDecimalToInt(tlDecimalDiv(tlDecimalFromInt(1032), tlDecimalFromInt(31))) == 1032 / 31);

    r = tlDecimalDiv(n1, n2);
    REQUIRE(tlDecimalToInt(r) == 109 / 274);

    r = tlDecimalDiv(n2, n1);
    REQUIRE(tlDecimalToInt(r) == 274 / 109);
    REQUIRE(tlDecimalToInt(tlDecimalDiv(tlDecimalFromInt(19), tlDecimalFromInt(378))) == 19 / 378);
    REQUIRE(tlDecimalToInt(tlDecimalDiv(tlDecimalFromInt(378), tlDecimalFromInt(19))) == 378 / 19);
}

TEST(mul) {
    char buf[1024];
    tlDecimal* d = tlDecimalMul(tlDecimalFromInt(101), tlDecimalFrom(1, 5));
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.00101"));

    d = tlDecimalMul(tlDecimalFrom(101, 2), tlDecimalFrom(33, 5));
    tlDecimalToString(d, buf, sizeof(buf), 10);
    print("RES: %s", buf);
    REQUIRE(!strcmp(buf, "0.0003333"));
}

int main(int argc, char** argv) {
    tl_init();
    RUN(build_and_print);
    RUN(basic);
    RUN(mul);
}

