// author: Onne Gorter, license: MIT (see license.txt)

// the vm itself, this is the library starting point

#include "../llib/lqueue.h"
#include "../llib/lhashmap.h"

#include "platform.h"
#include "vm.h"

#include "../llib/lqueue.c"
#include "../llib/lhashmap.c"

#include "../boot/init.tlb.h"
#include "../boot/compiler.tlb.h"

#include "buffer.h"
#include "value.h"
#include "number.h"
#include "string.h"
#include "bin.h"
#include "sym.h"
#include "list.h"
#include "set.h"
#include "object.h"
#include "map.h"

// evaluator internals
#include "frame.h"
#include "error.h"
#include "args.h"
#include "native.h"
#include "worker.h"
#include "task.h"
#include "lock.h"
#include "mutable.h"
#include "eval.h"
#include "bcode.h"
#include "env.h"
#include "debugger.h"
#include "var.h"
#include "array.h"
#include "hashmap.h"
#include "controlflow.h"

// extras
#include "queue.h"
#include "evio.h"
#include "time.h"
#include "regex.h"
#include "serialize.h"
#include "idset.h"
#include "weakmap.h"

void tlDumpTaskTrace();

static bool didbacktrace = false;

__attribute__((__noreturn__)) void tlabort() {
    if (didbacktrace) abort();
    didbacktrace = true;

    void *bt[128];
    char **strings;

    int size = backtrace(bt, 128);
    strings = backtrace_symbols(bt, size);

    fprintf(stderr, "\nfatal error; backtrace:\n");
    for (int i = 0; i < size; i++) {
        fprintf(stderr, "%s\n", strings[i]);
    }
    fflush(stderr);

    tlDumpTaskTrace();

    abort();
}

static bool didabort = false;
void tlbacktrace_fatal(int x) {
    if (didabort) return;
    didabort = true;
    tlabort();
}

tlHandle _dlopen(tlTask* task, tlArgs* args);

// return a hash
uint32_t tlHandleHash(tlHandle v, tlHandle* unhashable) {
    tlKind* kind = tl_kind(v);
    if (!kind->hash) {
        if (unhashable) *unhashable = v;
        if (!unhashable) warning("no hash for kind: %s", kind->name);
        return 0;
    }
    return kind->hash(v, unhashable);
}
// return true if equal, false otherwise
bool tlHandleEquals(tlHandle left, tlHandle right) {
    if (left == right) return true;
    tlKind* kleft = tl_kind(left);
    tlKind* kright = tl_kind(right);
    if (kleft != kright) {
        if (kleft == tlIntKind && kright == tlFloatKind) return tlFloatKind->equals(left, right);
        if (kleft == tlFloatKind && kright == tlIntKind) return tlFloatKind->equals(left, right);
        if (kleft == tlCharKind && kright == tlFloatKind) return tlFloatKind->equals(left, right);
        if (kleft == tlFloatKind && kright == tlCharKind) return tlFloatKind->equals(left, right);
        if (kleft == tlCharKind && kright == tlIntKind) return tlFloatKind->equals(left, right);
        if (kleft == tlIntKind && kright == tlCharKind) return tlFloatKind->equals(left, right);
        if (kleft == tlSymKind && kright == tlStringKind) return tlSymKind->equals(left, right);
        if (kleft == tlStringKind && kright == tlSymKind) return tlSymKind->equals(left, right);
        if (kleft == tlBinKind && kright == tlStringKind) return tlBinKind->equals(left, right);
        if (kleft == tlStringKind && kright == tlBinKind) return tlBinKind->equals(left, right);
        if (kleft == tlBinKind && kright == tlSymKind) return tlBinKind->equals(left, tlStringFromSym(right));
        if (kleft == tlSymKind && kright == tlBinKind) return tlBinKind->equals(tlStringFromSym(left), right);
        return false;
    }
    if (!kleft->equals) return false;
    return kleft->equals(left, right);
}
// return left - right
tlHandle tlCOMPARE(intptr_t i) {
    if (i < 0) return tlSmaller;
    if (i > 0) return tlLarger;
    return tlEqual;
}
// TODO this makes absolute ordering depending on runtime pointers ...
tlHandle tlHandleCompare(tlHandle left, tlHandle right) {
    if (tlUndefinedIs(left)) return left;
    if (tlUndefinedIs(right)) return right;

    if (left == right) return tlEqual;
    tlKind* kleft = tl_kind(left);
    tlKind* kright = tl_kind(right);
    if (kleft != kright) {
        if (kleft == tlIntKind && kright == tlFloatKind) return tlFloatKind->cmp(left, right);
        if (kleft == tlFloatKind && kright == tlIntKind) return tlFloatKind->cmp(left, right);
        if (kleft == tlCharKind && kright == tlFloatKind) return tlFloatKind->cmp(left, right);
        if (kleft == tlFloatKind && kright == tlCharKind) return tlFloatKind->cmp(left, right);
        if (kleft == tlCharKind && kright == tlIntKind) return tlFloatKind->cmp(left, right);
        if (kleft == tlIntKind && kright == tlCharKind) return tlFloatKind->cmp(left, right);

        if (kleft == tlIntKind && kright == tlNumKind) return tlNumKind->cmp(left, right);
        if (kleft == tlNumKind && kright == tlIntKind) return tlNumKind->cmp(left, right);
        if (kleft == tlCharKind && kright == tlNumKind) return tlNumKind->cmp(left, right);
        if (kleft == tlNumKind && kright == tlCharKind) return tlNumKind->cmp(left, right);
        if (kleft == tlFloatKind && kright == tlNumKind) return tlNumKind->cmp(left, right);
        if (kleft == tlNumKind && kright == tlFloatKind) return tlNumKind->cmp(left, right);

        if (kleft == tlSymKind && kright == tlStringKind) return tlSymKind->cmp(left, right);
        if (kleft == tlStringKind && kright == tlSymKind) return tlSymKind->cmp(left, right);
        if (kleft == tlBinKind && kright == tlStringKind) return tlBinKind->cmp(left, right);
        if (kleft == tlStringKind && kright == tlBinKind) return tlBinKind->cmp(left, right);
        if (kleft == tlBinKind && kright == tlSymKind) return tlBinKind->cmp(left, tlStringFromSym(right));
        if (kleft == tlSymKind && kright == tlBinKind) return tlBinKind->cmp(tlStringFromSym(left), right);
        return null;
        // TODO maybe return undefined because compare cannot be done here?
        // but actually, we need to let the interpreter execute user level code here
        // return tlCOMPARE((intptr_t)kleft - (intptr_t)kright);
    }
    if (!kleft->cmp) return tlCOMPARE((intptr_t)left - (intptr_t)right);
    tlHandle res = kleft->cmp(left, right);
    assert(res == tlSmaller || res == tlEqual || res == tlLarger || res == null);
    return res;
}

static tlHandle _out(tlTask* task, tlArgs* args) {
    trace("out(%d)", tlArgsSize(args));
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        printf("%s", tlStringData(tltoString(v)));
    }
    fflush(stdout);
    return tlNull;
}

static tlHandle _type(tlTask* task, tlArgs* args) {
    tlHandle c = tlArgsGet(args, 0);
    if (tlUserObjectIs(c)) return tlUserClassFor(c);
    tlKind* kind = tl_kind(c);
    if (kind->cls) return kind->cls;
    if (kind->klass) return kind->klass; // TOOD for now only
    return tlNull;
}

//. bool(v, t=true, f=false): return #t unless v is null or false or undefined
static tlHandle _bool(tlTask* task, tlArgs* args) {
    tlHandle c = tlArgsGet(args, 0);
    if (tl_bool(c)) {
        tlHandle res = tlArgsGet(args, 1);
        return res? res : tlTrue;
    }
    tlHandle res = tlArgsGet(args, 2);
    return res? res : tlFalse;
}

static tlHandle _not(tlTask* task, tlArgs* args) {
    trace("!%s", tl_str(tlArgsGet(args, 0)));
    return tlBOOL(!tl_bool(tlArgsGet(args, 0)));
}
static tlHandle _eq(tlTask* task, tlArgs* args) {
    trace("%p == %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(tlHandleEquals(tlArgsGet(args, 0), tlArgsGet(args, 1)));
}
static tlHandle _neq(tlTask* task, tlArgs* args) {
    trace("%p != %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(!tlHandleEquals(tlArgsGet(args, 0), tlArgsGet(args, 1)));
}
//. compare: compare two values, and always return a ordering, order will be runtime dependent sometimes
static tlHandle _compare(tlTask* task, tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    tlHandle cmp = tlHandleCompare(left, right);
    if (!cmp || tlUndefinedIs(cmp)) {
        tlKind* lk = tl_kind(left);
        tlKind* rk = tl_kind(right);
        if (lk->index != rk->index) return tlCOMPARE(rk->index - lk->index); // inverse ...
        int r = strcmp(lk->name, rk->name);
        if (r) return tlCOMPARE(r);

        // ultimate fallback, will depend on runtime pointers, cannot be equals
        if (tl_kind(left) < tl_kind(right)) return tlSmaller;
        return tlLarger;
    }
    assert(cmp == tlSmaller || cmp == tlEqual || cmp == tlLarger);
    return cmp;
}
static tlHandle _lt(tlTask* task, tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s < %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (tlUndefinedIs(res)) return res;
    if (!res) TL_UNDEF("cannot compare %s < %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res == tlSmaller);
}
static tlHandle _lte(tlTask* task, tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s <= %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (tlUndefinedIs(res)) return res;
    if (!res) TL_UNDEF("cannot compare %s <= %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res != tlLarger);
}
static tlHandle _gt(tlTask* task, tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s > %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (tlUndefinedIs(res)) return res;
    if (!res) TL_UNDEF("cannot compare %s > %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res == tlLarger);
}
static tlHandle _gte(tlTask* task, tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s >= %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (tlUndefinedIs(res)) return res;
    if (!res) TL_UNDEF("cannot compare %s >= %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res != tlSmaller);
}

// binary operators
static tlHandle _bnot(tlTask* task, tlArgs* args) {
    tlHandle v = tlArgsGet(args, 0);
    if (tlNumberIs(v)) {
        uint32_t vs = (uint32_t)tlNumberToInt64(v);
        uint32_t res = ~vs;
        return tlNUM((intptr_t)res);
    }
    TL_THROW("'~' not implemented for: ~%s", tl_str(v));
}
static tlHandle _band(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) {
        uint32_t lhs = (uint32_t)tlNumberToInt64(l);
        uint32_t rhs = (uint32_t)tlNumberToInt64(r);
        uint32_t res = lhs & rhs;
        return tlNUM((intptr_t)res);
    }
    TL_THROW("'&' not implemented for: %s & %s", tl_str(l), tl_str(r));
}
static tlHandle _bor(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) {
        uint32_t lhs = (uint32_t)tlNumberToInt64(l);
        uint32_t rhs = (uint32_t)tlNumberToInt64(r);
        uint32_t res = lhs | rhs;
        return tlNUM((intptr_t)res);
    }
    TL_THROW("'|' not implemented for: %s | %s", tl_str(l), tl_str(r));
}
static tlHandle _bxor(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) {
        uint32_t lhs = (uint32_t)tlNumberToInt64(l);
        uint32_t rhs = (uint32_t)tlNumberToInt64(r);
        uint32_t res = lhs ^ rhs;
        return tlNUM((intptr_t)res);
    }
    TL_THROW("'^' not implemented for: %s ^ %s", tl_str(l), tl_str(r));
}
// TODO shifting should work on arbitrary size number include tlNUM
static tlHandle _blshift(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) {
        int64_t lhs = tlNumberToInt64(l);
        int rhs = tl_int(r);
        if (rhs <= -64 || rhs >= 64) return tlZero;
        uint64_t res = rhs >= 0? lhs << rhs : lhs >> -rhs;
        return tlNUM((intptr_t)res);
    }
    TL_THROW("'<<' not implemented for: %s << %s", tl_str(l), tl_str(r));
}
static tlHandle _brshift(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) {
        int64_t lhs = tlNumberToInt64(l);
        int rhs = tl_int(r);
        if (rhs <= -64 || rhs >= 64) return tlZero;
        uint64_t res = rhs >= 0? lhs >> rhs : lhs << -rhs;
        return tlNUM((intptr_t)res);
    }
    TL_THROW("'>>' not implemented for: %s >> %s", tl_str(l), tl_str(r));
}
static tlHandle _brrshift(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) {
        uint32_t lhs = tlNumberToInt64(l);
        int rhs = tl_int(r);
        if (rhs < -64 || rhs > 64) return tlZero;
        uint32_t res = rhs >= 0? lhs >> rhs : lhs << -rhs;
        return tlNUM((uint32_t)res);
    }
    TL_THROW("'>>>' not implemented for: %s >>> %s", tl_str(l), tl_str(r));
}

// int/float/bigint
static tlHandle _neg(tlTask* task, tlArgs* args) {
    tlHandle v = tlArgsGet(args, 0);
    if (tlNullIs(v)) return tlINT(0);
    if (tlIntIs(v)) return tlINT(-tlIntToInt(v));
    if (tlFloatIs(v)) return tlFLOAT(-tl_double(v));
    if (tlNumberIs(v)) return tlNumNeg(tlNumTo(v));
    TL_THROW("'-' not implemented for: -%s", tl_str(v));
}
static tlHandle _add(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) {
        if (tlNullIs(r)) return tlNull;
        if (tlNumberIs(r)) return r;
        if (tlStringIs(r)) return r;
        if (tlBinIs(r)) return r;
        if (tlListIs(r)) return r;
    }
    if (tlNullIs(r)) {
        if (tlNumberIs(l)) return l;
        if (tlStringIs(l)) return l;
        if (tlBinIs(l)) return l;
        if (tlListIs(l)) return l;
    }
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tlIntToInt(l) + tlIntToInt(r));
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(tl_double(l) + tl_double(r));
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumAdd(tlNumTo(l), tlNumTo(r));
    if (tlBinIs(l) && tlBinIs(r)) return tlBinCat(tlBinAs(l), tlBinAs(r));
    if (tlNumberIs(l) && tlBinIs(r)) return tlBinFrom2(l, r);
    if (tlBinIs(l) && tlNumberIs(r)) return tlBinFrom2(l, r);

    // TODO when adding string and something else, convert else to string ...
    if (tlStringIs(l) && tlStringIs(r)) return tlStringCat(tlStringAs(l), tlStringAs(r));
    {
        tlList* ll = tlListCast(l);
        if (!ll && tlArgsIs(l)) ll = tlArgsList(tlArgsAs(l));
        tlList* rl = tlListCast(r);
        if (!rl && tlArgsIs(r)) rl = tlArgsList(tlArgsAs(r));
        if (ll && rl) return tlListCat(ll, rl);
    }
    TL_THROW("'+' not implemented for: %s + %s", tl_str(l), tl_str(r));
}
static tlHandle _sub(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) {
        if (tlNullIs(r)) return tlNull;
        if (tlNumberIs(r)) l = tlZero;
    }
    if (tlNullIs(r)) {
        if (tlNumberIs(l)) return l;
    }
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tlIntToInt(l) - tlIntToInt(r));
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(tl_double(l) - tl_double(r));
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumSub(tlNumTo(l), tlNumTo(r));
    TL_THROW("'-' not implemented for: %s - %s", tl_str(l), tl_str(r));
}
static tlHandle _mul(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) {
        if (tlNullIs(r)) return tlNull;
        if (tlNumberIs(r)) return tlZero;
    }
    if (tlNullIs(r)) {
        if (tlNumberIs(l)) return tlZero;
    }
    if (tlIntIs(l) && tlIntIs(r)) {
        // TODO figure out better limits, or let tlNum be smarter when to return small int
        if (tl_int(l) > -32000 && tl_int(l) < 32000 && tl_int(r) > -32000 && tl_int(r) < 32000) {
            return tlNUM(tl_int(l) * tl_int(r));
        }
    }
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(tl_double(l) * tl_double(r));
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumMul(tlNumTo(l), tlNumTo(r));
    TL_THROW("'*' not implemented for: %s * %s", tl_str(l), tl_str(r));
}
static tlHandle _div(tlTask* task, tlArgs* args) {
    // TODO normal div should return a big decimal
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) l = tlZero;
    if (tlNullIs(r)) r = tlZero;
    if (tlNumberIs(l) && tlNumberIs(r)) return tlFLOAT(tl_double(l) / tl_double(r));
    TL_THROW("'/' not implemented for: %s / %s", tl_str(l), tl_str(r));
}
static tlHandle _idiv(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) l = tlZero;
    if (tlNullIs(r)) r = tlZero;
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tlIntToInt(l) / tlIntToInt(r)); // TODO return remainder as well?
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumDiv(tlNumTo(l), tlNumTo(r));
    TL_THROW("'/' not implemented for: %s / %s", tl_str(l), tl_str(r));
}
static tlHandle _fdiv(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) l = tlZero;
    if (tlNullIs(r)) r = tlZero;
    if (tlNumberIs(l) && tlNumberIs(r)) return tlFLOAT(tl_double(l) / tl_double(r));
    TL_THROW("'/' not implemented for: %s / %s", tl_str(l), tl_str(r));
}
static tlHandle _mod(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(l)) l = tlZero;
    if (tlNullIs(r) || r == tlZero) r = tlFLOAT(0);
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tlIntToInt(l) % tlIntToInt(r));
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(fmod(tl_double(l), tl_double(r)));
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumMod(tlNumTo(l), tlNumTo(r));
    TL_THROW("'%%' not implemented for: %s %% %s", tl_str(l), tl_str(r));
}
static tlHandle _pow(tlTask* task, tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNullIs(r)) return tlOne;
    if (tlNullIs(l)) return tlZero;
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(pow(tl_double(l), tl_double(r)));
    if (tlNumberIs(l) && tlNumberIs(r)) {
        int p = tl_int(r); // TODO check if really small? or will that work out as zero anyhow?
        if (p < 0) return tlFLOAT(pow(tl_double(l), p));
        if (p < 1000) return tlNumPow(tlNumTo(l), p);
        TL_THROW("'**' out of range: %s ** %d", tl_str(l), p);
    }
    TL_THROW("'**' not implemented for: %s ** %s", tl_str(l), tl_str(r));
}
static tlHandle _sqrt(tlTask* task, tlArgs* args) {
    tlHandle a = tlArgsGet(args, 0);
    if (tlNullIs(a)) return tlZero;
    if (tlNumberIs(a)) {
        double res = sqrt(tl_double(a));
        return tlFLOAT(res);
    }
    TL_THROW("'sqrt' not implemented for: %s", tl_str(a));
}

//. random(n):
//. return number between 0 upto n (including 0, but not including n)
static tlHandle _random(tlTask* task, tlArgs* args) {
    // TODO fix this when we have floating point ...
    int upto = tl_int_or(tlArgsGet(args, 0), 0x3FFFFFFF);
    if (upto <= 0) return tlINT(0);
    int res = random() % upto;
    trace("RND: %d", res);
    return tlINT(res);
}
static tlHandle _sin(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(sin(in));
}
static tlHandle _cos(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(cos(in));
}
static tlHandle _tan(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(tan(in));
}
static tlHandle _asin(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(asin(in));
}
static tlHandle _acos(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(acos(in));
}
static tlHandle _atan(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(atan(in));
}
static tlHandle _atan2(tlTask* task, tlArgs* args) {
    double a = tl_double(tlArgsGet(args, 0));
    double b = tl_double(tlArgsGet(args, 1));
    return tlFLOAT(atan2(a, b));
}
static tlHandle _sinh(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(sinh(in));
}
static tlHandle _cosh(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(cosh(in));
}
static tlHandle _tanh(tlTask* task, tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(tanh(in));
}

// TODO remove and use tlNUMBER or such instead?
static tlHandle _int_parse(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");
    int base = tl_int_or(tlArgsGet(args, 1), 10);
    if (base < 2 || base > 36) TL_THROW("invalid base");
    const char* begin = tlStringData(str);
    char* end;
    long long res = strtol(begin, &end, base);
    if (end == begin) return tlNull;
    return tlNUM(res);
}

static tlHandle _base64encode(tlTask* task, tlArgs* args) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    //static const char* u64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    tlString* str = tlStringCast(tlArgsGet(args, 0));
    tlBin* bin = tlBinCast(tlArgsGet(args, 0));
    if (!str && !bin) TL_THROW("expect a String or Bin");

    const uint8_t* data = str? (const uint8_t*)tlStringData(str) : (const uint8_t*)tlBinData(bin);
    int len = str? tlStringSize(str) : tlBinSize(bin);

    int pad = len % 3;
    uint8_t* out = malloc_atomic((len / 3) * 4 + pad + 1);

    int i = 0;
    int j = 0;
    for (; i < len - pad; i += 3, j += 4) {
        uint32_t n = 0;
        n |= data[i + 0] << 16;
        n |= data[i + 1] <<  8;
        n |= data[i + 2] <<  0;

        out[j + 0] = b64[(n >> 18) & 0x3F];
        out[j + 1] = b64[(n >> 12) & 0x3F];
        out[j + 2] = b64[(n >>  6) & 0x3F];
        out[j + 3] = b64[(n >>  0) & 0x3F];
    }

    for (; i < len; i += 3, j += 4) {
        uint32_t n = 0;
        n |= i + 0 < len? data[i + 0] << 16 : 0;
        n |= i + 1 < len? data[i + 1] <<  8 : 0;
        n |= i + 2 < len? data[i + 2] <<  0 : 0;

        out[j + 0] = i + 0 < len? b64[(n >> 18) & 0x3F] : '=';
        out[j + 1] = i + 0 < len? b64[(n >> 12) & 0x3F] : '=';
        out[j + 2] = i + 1 < len? b64[(n >>  6) & 0x3F] : '=';
        out[j + 3] = i + 2 < len? b64[(n >>  0) & 0x3F] : '=';
    }
    out[j] = 0;
    return tlStringFromTake((char*)out, j);
}

static tlHandle _base64decode(tlTask* task, tlArgs* args) {

    static const unsigned char lookup[256] = {
    //  .0  .1  .2  .3   .4  .5  .6  .7    .8  .9  .A  .B   .C  .D  .E  .F
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // 0.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // 1.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 62,  64, 62,128, 63, // 2. 2D = - = 62
        52, 53, 54, 55,  56, 57, 58, 59,   60, 61, 64, 64,  64,128, 64, 64, // 3.
        64,  0,  1,  2,   3,  4,  5,  6,    7,  8,  9, 10,  11, 12, 13, 14, // 4.
        15, 16, 17, 18,  19, 20, 21, 22,   23, 24, 25, 64,  64, 64, 64, 63, // 5. 5F = _ = 63
        64, 26, 27, 28,  29, 30, 31, 32,   33, 34, 35, 36,  37, 38, 39, 40, // 6.
        41, 42, 43, 44,  45, 46, 47, 48,   49, 50, 51, 64,  64, 64, 64, 64, // 7.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // 8.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // 9.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // A.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // B.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // C.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // D.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64, // E.
        64, 64, 64, 64,  64, 64, 64, 64,   64, 64, 64, 64,  64, 64, 64, 64  // F.
    };

    tlString* str = tlStringCast(tlArgsGet(args, 0));

    int len = tlStringSize(str);
    if (len == 0) return tlStringEmpty();
    if (len % 4 != 0) TL_THROW("invalid base64");

    const unsigned char* data = (const unsigned char*)tlStringData(str);
    int padding = (lookup[data[len - 1]] == 128) + (lookup[data[len - 2]] == 128);
    if (padding == 2 && data[len - 1] != data[len - 2]) TL_THROW("invalid base64");
    int outlen = len * 3 / 4 - padding;

    unsigned char* out = malloc_atomic(outlen + padding); // a bit over allocated, so we can safely decode

    int i = 0;
    int j = 0;
    for (; i < len - 4; i += 4, j += 3) {
        unsigned char d0 = lookup[data[i + 0]];
        unsigned char d1 = lookup[data[i + 1]];
        unsigned char d2 = lookup[data[i + 2]];
        unsigned char d3 = lookup[data[i + 3]];
        if ((d0 | d1 | d2 | d3) & 0xC0) TL_THROW("invalid base64");
        out[j + 0] = d0 << 2 | d1 >> 4;
        out[j + 1] = d1 << 4 | d2 >> 2;
        out[j + 2] = d2 << 6 | d3 >> 0;
    }

    unsigned char d0 = lookup[data[i + 0]];
    unsigned char d1 = lookup[data[i + 1]];
    unsigned char d2 = lookup[data[i + 2]];
    unsigned char d3 = lookup[data[i + 3]];
    if ((d0 | d1 | d2 | d3) & 0x40) TL_THROW("invalid base64"); // here we allow = or .
    out[j + 0] = d0 << 2 | d1 >> 4;
    out[j + 1] = d1 << 4 | d2 >> 2;
    out[j + 2] = d2 << 6 | d3 >> 0;

    out[outlen] = 0;
    return tlStringFromTake((char*)out, outlen);
}

static tlHandle _urlencode(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");

    const char* data = tlStringData(str);
    int len = tlStringSize(str);
    int plen = 0;
    for (int i = 0; i < len; i++) {
        char c = data[i];
        if (!(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')) plen += 2;
    }

    char* res = malloc_atomic(len + plen);
    int j = 0;
    for (int i = 0; i < len; i++) {
        char c = data[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            res[j++] = c;
        } else {
            static char hex[] = "0123456789ABCDEF";
            res[j++] = '%';
            res[j++] = hex[(c >> 4) & 0x0F];
            res[j++] = hex[c & 0x0F];
        }
    }

    res[j] = 0;
    return tlStringFromTake(res, j);
}

static tlHandle _urldecode(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");

    const char* data = tlStringData(str);
    int len = tlStringSize(str);
    int percent = 0;
    for (int i = 0; i < len; i++) if (data[i] == '%') percent++;
    if (!percent) return str;

    char* res = malloc_atomic(len);
    int i;
    int j = 0;
    for (i = 0; i < len; i++) {
        if (data[i] != '%') { res[j++] = data[i]; continue; }
        if (!(i < len - 2)) break;
        char c = 0;
        char c1 = data[++i];
        char c2 = data[++i];

        if (c1 >= '0' && c1 <= '9') {
            c += (c1 - '0') * 16;
        } else if (c1 >= 'A' && c1 <= 'F') {
            c += (10 + c1 - 'A') * 16;
        } else if (c1 >= 'a' && c1 <= 'f') {
            c += (10 + c1 - 'a') * 16;
        } else {
            i -= 2;
            continue;
        }

        if (c2 >= '0' && c2 <= '9') {
            c += c2 - '0';
        } else if (c2 >= 'A' && c2 <= 'F') {
            c += 10 + c2 - 'A';
        } else if (c2 >= 'a' && c2 <= 'f') {
            c += 10 + c2 - 'a';
        } else {
            i -= 2;
            continue;
        }

        // TODO validate chars, how not to overflow utf8 bytes?
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') continue;
        if (c > 127) continue;

        res[j++] = c;
    }
    res[j] = 0;
    return tlStringFromTake(res, j);
}

void tl_init() {
    static bool tl_inited;
    if (tl_inited) return;
    tl_inited = true;

#ifdef HAVE_LOCALE
    setlocale(LC_NUMERIC, "C");
#endif


#ifdef HAVE_BOEHMGC
#if GC_VERSION_MAJOR >= 7 && GC_VERSION_MINOR >= 4
    GC_set_handle_fork(0);
    GC_set_dont_expand(0);
#endif
    GC_set_free_space_divisor(1); // default is 3; space vs collections, lower is more space
    GC_init();
#endif
    signal(SIGSEGV, tlbacktrace_fatal);
    signal(SIGBUS, tlbacktrace_fatal);
    signal(SIGSYS, tlbacktrace_fatal);

    class_init_first();
    sym_init_first();
    string_init_first();
    set_init_first();
    native_init_first();

    // assert assumptions on memory layout, pointer size etc
    //assert(sizeof(tlHead) <= sizeof(intptr_t));
    assert(!tlSymIs(tlFalse));
    assert(!tlSymIs(tlTrue));
    assert(tl_kind(tlTrue) == tlBoolKind);
    assert(tl_kind(tlFalse) == tlBoolKind);

    trace("    field size: %zd", sizeof(tlHandle));
    trace(" call overhead: %zd (%zd)", sizeof(tlCall), sizeof(tlCall)/sizeof(tlHandle));
    trace("frame overhead: %zd (%zd)", sizeof(tlFrame), sizeof(tlFrame)/sizeof(tlHandle));
    trace(" task overhead: %zd (%zd)", sizeof(tlTask), sizeof(tlTask)/sizeof(tlHandle));

    sym_string_init();

    set_init();
    object_init();
    list_init();

    class_init();
    map_init();
    bin_init();


    value_init();
    number_init();
    args_init();
    native_init();

    env_init();
    eval_init();
    frame_init();
    bcode_init();
    task_init();
    error_init();
    queue_init();

    var_init();
    array_init();
    hashmap_init();
    controlflow_init();
    mutable_init();
    vm_init();
    debugger_init();

    buffer_init();
    evio_init();
    time_init();
    serialize_init();

    idset_init();
    weakmap_init();

    assert(!tlArgsIs(0));
    assert(tlArgsAs(0) == 0);
}

void hotelparser_init();
void jsonparser_init();
void xmlparser_init();

tlVm* tlVmNew(tlSym procname, tlArgs* args) {
    srandom(time(0));
    tlVm* vm = tlAlloc(tlVmKind, sizeof(tlVm));
    vm->waiter = tlWorkerNew(vm);
    vm->globals = tlObjectEmpty();
    vm->procname = tlOR_NULL(procname);
    vm->args = tlOR_NULL(args);

    error_vm_default(vm);
    env_vm_default(vm);
    task_vm_default(vm);
    queue_vm_default(vm);
    var_vm_default(vm);
    evio_vm_default(vm);
    buffer_init_vm(vm);
    regex_init_vm(vm);

    hotelparser_init();
    jsonparser_init();
    xmlparser_init();

    vm->locals = tlObjectFrom("cwd", tl_cwd, null);
    vm->exitcode = -1;
    return vm;
}

void tlVmDelete(tlVm* vm) {
#ifdef HAVE_BOEHMGC
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
    GC_gcollect();
#endif
    free(vm);
}

int tlVmExitCode(tlVm* vm) {
    if (vm->exitcode >= 0) return vm->exitcode;
    if (vm->main && tlTaskIsDone(vm->main)) {
        return tlTaskGetThrowValue(vm->main)?1:0;
    }
    return 0;
}

void tlVmSetExitCode(tlVm* vm, int code) {
    assert(code >= 0);
    assert(vm->exitcode == -1);
    vm->exitcode = code;
}

static tlHandle _set_exitcode(tlTask* task, tlArgs* args) {
    int code = tl_int_or(tlArgsGet(args, 0), 1);
    tlVm* vm = tlVmCurrent(task);
    if (vm->exitcode >= 0) warning("exitcode already set: %d new: %d", vm->exitcode, code);
    vm->exitcode = code;
    return tlNull;
}

tlHandle tlVmGlobalGet(tlVm* vm, tlSym key) {
    return tlObjectGet(vm->globals, key);
}

void tlVmGlobalSet(tlVm* vm, tlSym key, tlHandle v) {
    assert(tlSymIs_(key));
    trace("%s = %s", tl_str(key), tl_str(v));
    vm->globals = tlObjectSet(vm->globals, key, v);
    assert(tlVmGlobalGet(vm, key) == v);
}

tlObject* tlVmGlobals(tlVm* vm) {
    return vm->globals;
}

tlBuffer* tlVmGetInit() {
    tlBuffer* buf = tlBufferNew();
    tlBufferWrite(buf, modules_init, modules_init_len);
    return buf;
}

tlBuffer* tlVmGetCompiler() {
    tlBuffer* buf = tlBufferNew();
    tlBufferWrite(buf, modules_compiler, modules_compiler_len);
    return buf;
}

extern tlBModule* g_init_module;

tlTask* tlVmEvalInitBuffer(tlVm* vm, tlBuffer* buf, const char* name, tlArgs* args) {
    const char* error = null;
    tlBModule* mod = tlBModuleFromBuffer(buf, tlSTR(name), &error);
    if (error) fatal("error loading init: %s", error);
    g_init_module = mod;

    tlBModuleLink(mod, tlVmGlobals(vm), &error);
    if (error) fatal("error linking init: %s", error);

    tlTask* task = tlBModuleCreateTask(vm, mod, args);
    tlVmRun(vm, task);
    return task;
}

tlTask* tlVmEvalInit(tlVm* vm, tlArgs* args) {
    return tlVmEvalInitBuffer(vm, tlVmGetInit(), "<init>", args);
}

static tlHandle _vm_get_compiler(tlTask* task, tlArgs* args) {
    return tlVmGetCompiler();
}

tlHandle _undefined(tlTask* task, tlArgs* args);
tlHandle _bless(tlTask* task, tlArgs* args);
tlHandle _Buffer_new(tlTask* task, tlArgs* args);

static const tlNativeCbs __vm_natives[] = {
    { "_undefined", _undefined },
    { "out",  _out },
    { "bool", _bool },
    { "type", _type },

    { "eq",   _eq },
    { "neq",  _neq },
    { "not",  _not },
    { "neg",  _neg },

    { "compare", _compare },
    { "lt",   _lt },
    { "lte",  _lte },
    { "gt",   _gt },
    { "gte",  _gte },

    { "bnot",  _bnot },
    { "band",  _band },
    { "bor",   _bor },
    { "bxor",  _bxor },
    { "lsh",  _blshift },
    { "rsh",  _brshift },
    { "rrsh", _brrshift },

    { "add",  _add },
    { "sub",  _sub },
    { "mul",  _mul },
    { "div",  _div },
    { "idiv",  _idiv },
    { "fdiv",  _fdiv },
    { "mod",  _mod },
    { "pow", _pow },

    { "random", _random },

    { "sqrt", _sqrt },
    { "sin", _sin },
    { "cos", _cos },
    { "tan", _tan },
    { "asin", _asin },
    { "acos", _acos },
    { "atan", _atan },
    { "atan2", _atan2 },
    { "sinh", _sinh },
    { "cosh", _cosh },
    { "tanh", _tanh },

    { "_bless", _bless },

    { "_int_parse", _int_parse },
    { "_urlencode", _urlencode },
    { "_urldecode", _urldecode },
    { "_base64encode", _base64encode },
    { "_base64decode", _base64decode },

    { "_Buffer_new", _Buffer_new },
    { "_vm_get_compiler", _vm_get_compiler },

    { "_set_exitcode", _set_exitcode },
    { "dlopen", _dlopen },

    { 0, 0 },
};

static tlKind _tlVmKind = {
    .name = "Vm"
};
static tlKind _tlWorkerKind = {
    .name = "Worker"
};
tlKind* tlVmKind;
tlKind* tlWorkerKind;

void vm_init() {
    tl_register_natives(__vm_natives);
    tlObject* system = tlObjectFrom(
            "prefix", tlSTR(TL_PREFIX),
            "version", tlSTR(TL_VERSION),
            "platform", tlSTR(PLATFORM),
            "cpu", tlSTR(CPU),
#ifdef M32
            "wordsize", tlINT(32),
#else
            "wordsize", tlINT(64),
#endif
            null);
    tl_register_global("system", system);

    INIT_KIND(tlVmKind);
    INIT_KIND(tlWorkerKind);
}

void tlVmRun(tlVm* vm, tlTask* task) {
    tlWorker* worker = tlWorkerNew(vm);
    vm->main = task;
    vm->running = true;

    tlWorkerRun(worker);
}

// TODO tl_str is for debugging, and this should all be done in language
static tlHandle _print(tlTask* task, tlArgs* args) {
    const char* sep = " ";
    tlHandle v = tlArgsGetNamed(args, tlSYM("sep"));
    if (v) {
        sep = tl_bool(v)? tl_str(v) : "";
    }

    const char* begin = "";
    v = tlArgsGetNamed(args, tlSYM("begin"));
    if (v) {
        begin = tl_bool(v)? tl_str(v) : "";
    }

    const char* end = "\n";
    v = tlArgsGetNamed(args, tlSYM("end"));
    if (v) {
        end = tl_bool(v)? tl_str(v) : "";
    }

    if (begin) printf("%s", begin);
    for (int i = 0; i < 1000; i++) {
        v = tlArgsGet(args, i);
        if (!v) break;
        if (i > 0) printf("%s", sep);
        printf("%s", tl_str(v));
    }
    printf("%s", end);
    fflush(stdout);
    return tlNull;
}

static tlHandle _assert(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGetNamed(args, tlSYM("string")));
    if (!str) str = tlStringEmpty();
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        if (!tl_bool(v)) TL_THROW("Assertion Failed: %s", tlStringData(str));
    }
    return tlNull;
}

void tlVmInitDefaultEnv(tlVm* vm) {
    tlNative* print = tlNativeNew(_print, tlSYM("print"));
    tlVmGlobalSet(vm, tlNativeName(print), print);

    tlNative* assert = tlNativeNew(_assert, tlSYM("assert"));
    tlVmGlobalSet(vm, tlNativeName(assert), assert);
}

#include <dlfcn.h>
typedef tlHandle(*tlCModuleLoad)();
tlHandle _dlopen(tlTask* task, tlArgs* args) {
    tlString* name = tlStringAs(tlArgsGet(args, 0));
    void* lib = dlopen(tlStringData(name), RTLD_NOW|RTLD_LOCAL);
    if (!lib) TL_THROW("dlopen: %s", dlerror());

    void* tl_load = dlsym(lib, "tl_load");
    if (!tl_load) {
        dlclose(lib);
        TL_THROW("dlopen: %s", dlerror());
    }

    tlHandle res = ((tlCModuleLoad)tl_load)();
    if (!res || !tl_kind(res)->name) {
        dlclose(lib);
        TL_THROW("dlopen: unable to initialize lib");
    }
    return res;
}

