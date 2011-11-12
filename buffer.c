// an actor based buffer for io

TL_REF_TYPE(tlBuffer);
struct tlBuffer {
    tlActor actor;
    tl_buf* buf;
};
static tlClass _tlBufferClass = {
    .name = "Buffer",
    .send = tlActorReceive,
};
tlClass* tlBufferClass = &_tlBufferClass;

tlBuffer* tlBufferNew(tlTask* task) {
    tlBuffer* buf = tlAlloc(task, tlBufferClass, sizeof(tlBuffer));
    buf->buf = tlbuf_new();
    return buf;
}

INTERNAL tlValue _BufferRead(tlTask* task, tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(args->target);
    assert(buffer);
    tl_buf* buf = buffer->buf;
    assert(buf);

    trace("canread: %d", canread(buf));
    char* data = malloc(canread(buf) + 1);
    int last = tlbuf_read(buf, data, canread(buf));
    data[last] = 0;
    return tlTextNewTake(task, data, last);
}

INTERNAL tlValue _BufferWrite(tlTask* task, tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(args->target);
    assert(buffer);
    tl_buf* buf = buffer->buf;
    assert(buf);

    tlText* text = tlTextCast(tlArgsAt(args, 0));
    if (!text) TL_THROW("expected a Text");
    return tlINT(tlbuf_write(buf, tlTextData(text), tlTextSize(text)));
}

INTERNAL tlValue _Buffer_new(tlTask* task, tlArgs* args) {
    return tlBufferNew(task);
}

static void buffer_init() {
    _tlBufferClass.map = tlClassMapFrom(
            "read", _BufferRead,
            "write", _BufferWrite,
            null
    );
}

