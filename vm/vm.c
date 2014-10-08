// the vm itself, this is the library starting point

#include "tl.h"
#include "platform.h"
#include "boot/boot.tlb.h"
#include "boot/compiler.tlb.h"

#include "llib/lhashmap.c"
#include "llib/lqueue.c"

#include "debug.h"
#include "debug.c"
#include "buffer.c"

#include "value.c"
#include "number.c"
#include "string.c"
#include "bin.c"
#include "sym.c"
#include "list.c"
#include "set.c"
#include "object.c"
#include "map.c"

// evaluator internals
#include "frame.c"
#include "error.c"
#include "args.c"
#include "call.c"
#include "worker.c"
#include "task.c"
#include "lock.c"
#include "mutable.c"
#include "code.c"
#include "env.c"
#include "eval.c"

#include "bcode.c"
#include "debugger.c"

#include "var.c"
#include "array.c"
#include "hashmap.c"
#include "controlflow.c"
#include "queue.c"

// extras
#include "evio.c"
#include "time.c"
#include "regex.c"
#include "serialize.c"

//#include "storage.c"

#include "trace-off.h"

tlHandle _dlopen(tlArgs* args);

// return a hash
unsigned tlHandleHash(tlHandle v) {
    tlKind* kind = tl_kind(v);
    if (!kind->hash) fatal("no hash for kind: %s", kind->name);
    return kind->hash(v);
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
        if (kleft == tlBinKind && kright == tlSymKind) return tlBinKind->equals(left, _TEXT_FROM_SYM(right));
        if (kleft == tlSymKind && kright == tlBinKind) return tlBinKind->equals(_TEXT_FROM_SYM(left), right);
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
        if (kleft == tlBinKind && kright == tlSymKind) return tlBinKind->cmp(left, _TEXT_FROM_SYM(right));
        if (kleft == tlSymKind && kright == tlBinKind) return tlBinKind->cmp(_TEXT_FROM_SYM(left), right);
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

static tlHandle _out(tlArgs* args) {
    trace("out(%d)", tlArgsSize(args));
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        printf("%s", tlStringData(tltoString(v)));
    }
    fflush(stdout);
    return tlNull;
}

static tlHandle _bool(tlArgs* args) {
    tlHandle c = tlArgsGet(args, 0);
    trace("bool(%s)", tl_bool(c)?"true":"false");
    tlHandle res = tlArgsGet(args, tl_bool(c)?1:2);
    return tlVALUE_OR_NULL(res);
}
static tlHandle _not(tlArgs* args) {
    trace("!%s", tl_str(tlArgsGet(args, 0)));
    return tlBOOL(!tl_bool(tlArgsGet(args, 0)));
}
static tlHandle _eq(tlArgs* args) {
    trace("%p == %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(tlHandleEquals(tlArgsGet(args, 0), tlArgsGet(args, 1)));
}
static tlHandle _neq(tlArgs* args) {
    trace("%p != %p", tlArgsGet(args, 0), tlArgsGet(args, 1));
    return tlBOOL(!tlHandleEquals(tlArgsGet(args, 0), tlArgsGet(args, 1)));
}
/// compare: compare two values, and always return a ordering, order will be runtime dependend sometimes
static tlHandle _compare(tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    tlHandle cmp = tlHandleCompare(left, right);
    if (!cmp) return tlCOMPARE(tl_kind(left) - tl_kind(right));
    return cmp;
}
static tlHandle _lt(tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s < %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (!res) TL_UNDEF("cannot compare %s < %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res == tlSmaller);
}
static tlHandle _lte(tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s <= %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (!res) TL_UNDEF("cannot compare %s <= %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res != tlLarger);
}
static tlHandle _gt(tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s > %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (!res) TL_UNDEF("cannot compare %s > %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res == tlLarger);
}
static tlHandle _gte(tlArgs* args) {
    tlHandle left = tlArgsGet(args, 0); tlHandle right = tlArgsGet(args, 1);
    trace("%s >= %s", tl_str(left), tl_str(right));
    tlHandle res = tlHandleCompare(left, right);
    if (!res) TL_UNDEF("cannot compare %s >= %s", tl_str(left),  tl_str(right));
    assert(res == tlSmaller || res == tlEqual || res == tlLarger);
    return tlBOOL(res != tlSmaller);
}

// shift operators
static tlHandle _band(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) return tlINT(tl_int(l) & tl_int(r));
    TL_THROW("'&' not implemented for: %s & %s", tl_str(l), tl_str(r));
}
static tlHandle _bor(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) return tlINT(tl_int(l) | tl_int(r));
    TL_THROW("'|' not implemented for: %s | %s", tl_str(l), tl_str(r));
}
static tlHandle _lshift(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) return tlINT(tl_int(l) << tl_int(r));
    TL_THROW("'<<' not implemented for: %s << %s", tl_str(l), tl_str(r));
}
static tlHandle _rshift(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) return tlINT(tl_int(l) >> tl_int(r));
    TL_THROW("'>>' not implemented for: %s >> %s", tl_str(l), tl_str(r));
}

// int/float
static tlHandle _add(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tl_int(l) + tl_int(r));
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
static tlHandle _sub(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tl_int(l) - tl_int(r));
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(tl_double(l) - tl_double(r));
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumSub(tlNumTo(l), tlNumTo(r));
    TL_THROW("'-' not implemented for: %s - %s", tl_str(l), tl_str(r));
}
static tlHandle _mul(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
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
static tlHandle _div(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlNumberIs(l) && tlNumberIs(r)) return tlFLOAT(tl_double(l) / tl_double(r));
    TL_THROW("'/' not implemented for: %s / %s", tl_str(l), tl_str(r));
}
static tlHandle _idiv(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tl_int(l) / tl_int(r)); // TODO return remainder as well?
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumDiv(tlNumTo(l), tlNumTo(r));
    TL_THROW("'/' not implemented for: %s / %s", tl_str(l), tl_str(r));
}
static tlHandle _mod(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if (tlIntIs(l) && tlIntIs(r)) return tlNUM(tl_int(l) % tl_int(r));
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(tl_double(l) * tl_double(r));
    if (tlNumberIs(l) && tlNumberIs(r)) return tlNumMod(tlNumTo(l), tlNumTo(r));
    TL_THROW("'%%' not implemented for: %s %% %s", tl_str(l), tl_str(r));
}
static tlHandle _pow(tlArgs* args) {
    tlHandle l = tlArgsGet(args, 0); tlHandle r = tlArgsGet(args, 1);
    if ((tlFloatIs(l) && tlNumberIs(r)) || (tlNumberIs(l) && tlFloatIs(r))) return tlFLOAT(pow(tl_double(l), tl_double(r)));
    if (tlNumberIs(l) && tlNumberIs(r)) {
        int p = tl_int(r); // TODO check if really small? or will that work out as zero anyhow?
        if (p < 0) return tlFLOAT(pow(tl_double(l), p));
        if (p < 1000) return tlNumPow(tlNumTo(l), p);
        TL_THROW("'^' out of range: %s ^ %d", tl_str(l), p);
    }
    TL_THROW("'^' not implemented for: %s ^ %s", tl_str(l), tl_str(r));
}
static tlHandle _binand(tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) & tl_int(tlArgsGet(args, 1));
    trace("BIN AND: %d", res);
    return tlINT(res);
}
static tlHandle _binor(tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) | tl_int(tlArgsGet(args, 1));
    trace("BIN OR: %d", res);
    return tlINT(res);
}
static tlHandle _binxor(tlArgs* args) {
    int res = tl_int(tlArgsGet(args, 0)) ^ tl_int(tlArgsGet(args, 1));
    trace("BIN XOR: %d", res);
    return tlINT(res);
}
static tlHandle _binnot(tlArgs* args) {
    int res = ~tl_int(tlArgsGet(args, 0));
    trace("BIN NOT: %d", res);
    return tlINT(res);
}
static tlHandle _shift(tlArgs* args) {
    int from = tl_int(tlArgsGet(args, 0));
    int by = tl_int_or(tlArgsGet(args, 1), 0);
    int res;
    if (by < 0) {
        if (-by > sizeof(int)*8) { res = 0; } else { res = from >> -by; }
    } else if (by > 0) {
        if (by > sizeof(int)*8) { res = 0; } else { res = from << by; }
    } else {
        res = from;
    }
    trace("BIN SHIFT: %d by: %d == %d", from, by, res);
    return tlINT(res);
}
static tlHandle _sqrt(tlArgs* args) {
    double res = sqrt(tl_double(tlArgsGet(args, 0)));
    trace("SQRT: %d", res);
    return tlFLOAT(res);
}

/// random(n)
/// return number between 0 upto n (including 0, but not including n)
static tlHandle _random(tlArgs* args) {
    // TODO fix this when we have floating point ...
    int upto = tl_int_or(tlArgsGet(args, 0), 0x3FFFFFFF);
    if (upto <= 0) return tlINT(0);
    int res = random() % upto;
    trace("RND: %d", res);
    return tlINT(res);
}
static tlHandle _sin(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(sin(in));
}
static tlHandle _cos(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(cos(in));
}
static tlHandle _tan(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(tan(in));
}
static tlHandle _asin(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(asin(in));
}
static tlHandle _acos(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(acos(in));
}
static tlHandle _atan(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(atan(in));
}
static tlHandle _atan2(tlArgs* args) {
    double a = tl_double(tlArgsGet(args, 0));
    double b = tl_double(tlArgsGet(args, 1));
    return tlFLOAT(atan2(a, b));
}
static tlHandle _sinh(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(sinh(in));
}
static tlHandle _cosh(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(cosh(in));
}
static tlHandle _tanh(tlArgs* args) {
    double in = tl_double(tlArgsGet(args, 0));
    return tlFLOAT(tanh(in));
}

static tlHandle _int_parse(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");
    int base = tl_int_or(tlArgsGet(args, 1), 10);
    if (base < 2 || base > 36) TL_THROW("invalid base");
    const char* begin = tlStringData(str);
    char* end;
    long long res = strtol(begin, &end, base);
    if (end == begin) return tlNull;
    if (res >= TL_MIN_INT && res <= TL_MAX_INT) return tlINT(res);
    return tlNull;
}

static tlHandle _urldecode(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expect a String");

    const char* data = tlStringData(str);
    int len = tlStringSize(str);
    int percent = 0;
    for (int i = 0; i < len; i++) if (data[i] == '%') percent++;
    if (!percent) return str;

    char* res = malloc(len);
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
            c += c2 - 'A';
        } else if (c2 >= 'a' && c2 <= 'f') {
            c += c2 - 'a';
        } else {
            i -= 2;
            continue;
        }
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') continue;
        res[j++] = c;
    }
    res[j] = 0;
    return tlStringFromTake(res, j);
}

static void vm_init();

void tl_init() {
    static bool tl_inited;
    if (tl_inited) return;
    tl_inited = true;

#ifdef HAVE_LOCALE
    setlocale(LC_NUMERIC, "C");
#endif


#ifdef HAVE_BOEHMGC
    GC_set_handle_fork(0);
    GC_set_dont_expand(0);
    GC_set_free_space_divisor(1); // default is 3; space vs collections, lower is more space
    GC_init();
#endif
    signal(SIGSEGV, tlbacktrace_fatal);
    signal(SIGBUS, tlbacktrace_fatal);
    signal(SIGSYS, tlbacktrace_fatal);

    INIT_KIND(tlSymKind);
    INIT_KIND(tlStringKind);
    INIT_KIND(tlSetKind);
    INIT_KIND(tlObjectKind);
    INIT_KIND(tlNativeKind);

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

    bin_init();
    list_init();
    set_init();
    object_init();
    map_init();


    value_init();
    number_init();
    args_init();
    call_init();

    env_init();
    eval_init();
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

    assert(!tlArgsIs(0));
    assert(tlArgsAs(0) == 0);
}

void hotelparser_init();
void jsonparser_init();
void xmlparser_init();

tlVm* tlVmNew() {
    srandom(time(NULL));
    tlVm* vm = tlAlloc(tlVmKind, sizeof(tlVm));
    vm->waiter = tlWorkerNew(vm);
    vm->globals = tlEnvNew(null);

    error_vm_default(vm);
    env_vm_default(vm);
    task_vm_default(vm);
    queue_vm_default(vm);
    var_vm_default(vm);
    array_vm_default(vm);
    hashmap_vm_default(vm);
    evio_vm_default(vm);
    buffer_init_vm(vm);
    regex_init_vm(vm);

    //openssl_init_vm(vm);
    //audio_init_vm(vm);
    //storage_init_vm(vm);
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

INTERNAL tlHandle _set_exitcode(tlArgs* args) {
    int code = tl_int_or(tlArgsGet(args, 0), 1);
    tlVm* vm = tlVmCurrent();
    if (vm->exitcode >= 0) warning("exitcode already set: %d new: %d", vm->exitcode, code);
    vm->exitcode = code;
    return tlNull;
}

void tlVmGlobalSet(tlVm* vm, tlSym key, tlHandle v) {
    vm->globals = tlEnvSet(vm->globals, key, v);
}

tlEnv* tlVmGlobalEnv(tlVm* vm) {
    return vm->globals;
}

tlBuffer* tlVmGetBoot() {
    tlBuffer* buf = tlBufferNew();
    tlBufferWrite(buf, boot_boot, boot_boot_len);
    return buf;
}

tlBuffer* tlVmGetCompiler() {
    tlBuffer* buf = tlBufferNew();
    tlBufferWrite(buf, modules_compiler, modules_compiler_len);
    return buf;
}

static tlHandle _vm_get_compiler() {
    return tlVmGetCompiler();
}

static const tlNativeCbs __vm_natives[] = {
    { "_undefined", _undefined },
    { "out",  _out },
    { "bool", _bool },
    { "not",  _not },

    { "eq",   _eq },
    { "neq",  _neq },

    { "compare", _compare },
    { "lt",   _lt },
    { "lte",  _lte },
    { "gt",   _gt },
    { "gte",  _gte },

    { "band",  _band },
    { "bor",  _bor },
    { "lsh",  _lshift },
    { "rsh",  _rshift },

    { "add",  _add },
    { "sub",  _sub },
    { "mul",  _mul },
    { "div",  _div },
    { "idiv",  _idiv },
    { "mod",  _mod },
    { "pow", _pow },

    { "binand",  _binand },
    { "binor",  _binor },
    { "binxor",  _binxor },
    { "binnot",  _binnot },
    { "shift", _shift },

    { "sqrt", _sqrt },
    { "random", _random },

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

    { "_int_parse", _int_parse },
    { "_urldecode", _urldecode },

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

static void vm_init() {
    tl_register_natives(__vm_natives);
    tlObject* system = tlObjectFrom(
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

static tlHandle _print(tlArgs* args) {
    tlString* sep = tlSTR(" ");
    tlHandle v = tlArgsMapGet(args, tlSYM("sep"));
    if (v) sep = tltoString(v);

    tlString* begin = null;
    v = tlArgsMapGet(args, tlSYM("begin"));
    if (v) begin = tltoString(v);

    tlString* end = null;
    v = tlArgsMapGet(args, tlSYM("end"));
    if (v) end = tltoString(v);

    if (begin) printf("%s", tlStringData(begin));
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        if (i > 0) printf("%s", tlStringData(sep));
        printf("%s", tlStringData(tltoString(v)));
    }
    if (end) printf("%s", tlStringData(end));
    printf("\n");
    fflush(stdout);
    return tlNull;
}

static tlHandle _assert(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsMapGet(args, tlSYM("string")));
    if (!str) str = tlStringEmpty();
    for (int i = 0; i < 1000; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) break;
        if (!tl_bool(v)) TL_THROW("Assertion Failed: %s", tlStringData(str));
    }
    return tlNull;
}

static tlHandle _error(tlArgs* args) {
    TL_THROW("hello world!");
}

void tlVmInitDefaultEnv(tlVm* vm) {
    tlNative* print = tlNativeNew(_print, tlSYM("print"));
    tlVmGlobalSet(vm, tlNativeName(print), print);

    tlNative* assert = tlNativeNew(_assert, tlSYM("assert"));
    tlVmGlobalSet(vm, tlNativeName(assert), assert);

    tlNative* eval = tlNativeNew(_eval, tlSYM("eval"));
    tlVmGlobalSet(vm, tlNativeName(eval), eval);

    tlNative* error = tlNativeNew(_error, tlSYM("error"));
    tlVmGlobalSet(vm, tlNativeName(error), error);
}

#include <dlfcn.h>
typedef tlHandle(*tlCModuleLoad)();
tlHandle _dlopen(tlArgs* args) {
    tlString* name = tlStringAs(tlArgsGet(args, 0));
    void* lib = dlopen(tlStringData(name), RTLD_NOW|RTLD_LOCAL);
    if (!lib) TL_THROW("dlopen: %s", dlerror());

    void* tl_load = dlsym(lib, "tl_load");
    if (!tl_load) TL_THROW("dlopen: %s", dlerror());

    tlHandle res = ((tlCModuleLoad)tl_load)();
    if (!res) TL_THROW("dlopen: unable to initialize lib");
    if (!tl_kind(res)->name) TL_THROW("dlopen: unable to initialize lib");
    return res;
}

