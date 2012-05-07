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
INTERNAL tlValue _buffer_canread(tlArgs* args) {
    return tlINT(canread(tlBufferAs(tlArgsTarget(args))->buf));
}
INTERNAL tlValue _buffer_canwrite(tlArgs* args) {
    return tlINT(canwrite(tlBufferAs(tlArgsTarget(args))->buf));
}
INTERNAL tlValue _buffer_read(tlArgs* args) {
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

INTERNAL tlValue _buffer_write(tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");
    // TODO grow buffer? auto grow buffer?
    return tlINT(tlbuf_write(buf, tlTextData(text), tlTextSize(text)));
}

INTERNAL tlValue _buffer_find(tlArgs* args) {
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

INTERNAL tlValue _Buffer_new(tlArgs* args) {
    return tlBufferNew();
}

static void buffer_init() {
    _tlBufferKind.map = tlClassMapFrom(
            "canread", _buffer_canread,
            "read", _buffer_read,
            "canwrite", _buffer_canwrite,
            "write", _buffer_write,
            "find", _buffer_find,
            null
    );
}

