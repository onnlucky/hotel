// a locked buffer, usable for io

TL_REF_TYPE(tlBuffer);
struct tlBuffer {
    tlLock lock;
    tl_buf* buf;
};
static tlKind _tlBufferKind = {
    .name = "Buffer",
    .locked = true,
};
tlKind* tlBufferKind = &_tlBufferKind;

tlBuffer* tlBufferNew() {
    tlBuffer* buf = tlAlloc(tlBufferKind, sizeof(tlBuffer));
    buf->buf = tlbuf_new();
    return buf;
}
INTERNAL tlHandle _buffer_canread(tlArgs* args) {
    return tlINT(canread(tlBufferAs(tlArgsTarget(args))->buf));
}
INTERNAL tlHandle _buffer_canwrite(tlArgs* args) {
    return tlINT(canwrite(tlBufferAs(tlArgsTarget(args))->buf));
}
INTERNAL tlHandle _buffer_read(tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    trace("canread: %d", canread(buf));
    char* data = malloc_atomic(canread(buf) + 1);
    int last = tlbuf_read(buf, data, canread(buf));
    data[last] = 0;
    return tlTextFromTake(data, last);
}

INTERNAL tlHandle _buffer_write(tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    tlHandle v = tlArgsGet(args, 0);
    if (tlTextIs(v)) {
        tlText* text = tlTextAs(v);
        return tlINT(tlbuf_write(buf, tlTextData(text), tlTextSize(text)));
    } else if (tlBufferIs(v)) {
        tlBuffer* tb = tlBufferAs(v); tl_buf* b = tb->buf; assert(b);
        int size = canread(b);
        tlbuf_write(buf, readbuf(b), size);
        didread(b, size);
        return tlINT(size);
    } else {
        TL_THROW("expected a Text or Buffer or Bin to write");
    }
}
INTERNAL tlHandle _buffer_writeByte(tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    int b = tl_int_or(tlArgsGet(args, 0), 0);
    tlbuf_write_uint8(buf, (uint8_t)b);
    return tlOne;
}

INTERNAL tlHandle _buffer_find(tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    int i = tlbuf_find(buf, tlTextData(text), tlTextSize(text));
    if (i < 0) return tlNull;
    return tlINT(i);
}

INTERNAL tlHandle _Buffer_new(tlArgs* args) {
    return tlBufferNew();
}
static tlHandle _isBuffer(tlArgs* args) {
    return tlBOOL(tlBufferIs(tlArgsGet(args, 0)));
}

static void buffer_init() {
    _tlBufferKind.klass = tlClassMapFrom(
            "canread", _buffer_canread,
            "size", _buffer_canread,
            "read", _buffer_read,
            "canwrite", _buffer_canwrite,
            "write", _buffer_write,
            "writeByte", _buffer_writeByte,
            "find", _buffer_find,
            null
    );
}

