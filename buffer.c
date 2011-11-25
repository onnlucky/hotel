// an synchronized buffer, usable for io

TL_REF_TYPE(tlBuffer);
struct tlBuffer {
    tlSynchronized sync;
    tl_buf* buf;
};
static tlClass _tlBufferClass = {
    .name = "Buffer",
    .send = tlSynchronizedReceive,
};
tlClass* tlBufferClass = &_tlBufferClass;

tlBuffer* tlBufferNew(tlTask* task) {
    tlBuffer* buf = tlAlloc(task, tlBufferClass, sizeof(tlBuffer));
    buf->buf = tlbuf_new();
    return buf;
}
INTERNAL tlValue _buffer_canread(tlTask* task, tlArgs* args) {
    return tlINT(canread(tlBufferAs(tlArgsTarget(args))->buf));
}
INTERNAL tlValue _buffer_canwrite(tlTask* task, tlArgs* args) {
    return tlINT(canwrite(tlBufferAs(tlArgsTarget(args))->buf));
}
INTERNAL tlValue _buffer_read(tlTask* task, tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    trace("canread: %d", canread(buf));
    char* data = malloc(canread(buf) + 1);
    int last = tlbuf_read(buf, data, canread(buf));
    data[last] = 0;
    return tlTextFromTake(task, data, last);
}

INTERNAL tlValue _buffer_write(tlTask* task, tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(tlArgsTarget(args));
    if (!buffer) TL_THROW("expected a Buffer");
    tl_buf* buf = buffer->buf;
    assert(buf);

    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");
    // TODO grow buffer? auto grow buffer?
    return tlINT(tlbuf_write(buf, tlTextData(text), tlTextSize(text)));
}

INTERNAL tlValue _Buffer_new(tlTask* task, tlArgs* args) {
    return tlBufferNew(task);
}

static void buffer_init() {
    _tlBufferClass.map = tlClassMapFrom(
            "canread", _buffer_canread,
            "read", _buffer_read,
            "canwrite", _buffer_canwrite,
            "write", _buffer_write,
            null
    );
}

