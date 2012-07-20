
void walk(tlHandle h) {
    tlKind* kind = tl_kind(h);
    if (kind == tlNullKind) print("null");
    if (kind == tlIntKind) print("%zd", tl_int(h));
    if (kind == tlTextKind) print("%s", tlTextData(tlTextAs(h)));
    if (kind == tlMapKind) print("object");
}

tlHandle _read_blob(tlArgs* args) {
    return tlNull;
}

tlHandle _write_blob(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    walk(h);
    return tlNull;
}

