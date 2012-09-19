
struct tlFloat {
    tlHead* head;
    double value;
};

tlFloat* tlFLOAT(double d) {
    tlFloat* f = tlAlloc(tlFloatKind, sizeof(tlFloat));
    f->value = d;
    return f;
}
double tl_double(tlHandle h) {
    if (tlFloatIs(h)) return tlFloatAs(h)->value;
    if (tlIntIs(h)) return tl_int(h);
    return NAN;
}
double tl_double_or(tlHandle h, double d) {
    if (tlFloatIs(h)) return tlFloatAs(h)->value;
    if (tlIntIs(h)) return tl_int(h);
    return d;
}

static const char* floatToText(tlHandle v, char* buf, int size) {
    snprintf(buf, size, "%f", tl_double(v)); return buf;
}
static unsigned int floatHash(tlHandle h) {
    return murmurhash2a((uint8_t*)&tlFloatAs(h)->value, sizeof(double));
}
static bool floatEquals(tlHandle left, tlHandle right) {
    return tl_double(left) == tl_double(right);
}
static int floatCmp(tlHandle left, tlHandle right) {
    double l = tl_double(left);
    double r = tl_double(right);
    if (l == r) return 0;
    if (l > r) return 1;
    return -1;
}
static tlKind _tlFloatKind = {
    .name = "Float",
    .toText = floatToText,
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
static tlHandle _float_toText(tlArgs* args) {
    double c = tl_double(tlArgsTarget(args));
    char buf[255];
    int len = snprintf(buf, sizeof(buf), "%f", c);
    return tlTextFromCopy(buf, len);
}

static void number_init() {
    _tlFloatKind.klass = tlClassMapFrom(
        "hash", _float_hash,
        "floor", _float_floor,
        "round", _float_round,
        "ceil", _float_ceil,
        "toText", _float_toText,
        null
    );
}

