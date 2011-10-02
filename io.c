// hotel simple io

static tlSym s_read;
static tlSym s_write;

static tlClass tlBufferClass;

typedef struct tlHotelBuffer {
    tlActor actor;
    tlBuffer* buf;
} tlHotelBuffer;

INTERNAL tlPause* _BufferAct(tlTask* task, tlArgs* args);

tlHotelBuffer* tlBufferNew() {
    tlHotelBuffer* buf = calloc(1, sizeof(tlHotelBuffer));
    buf->actor.head.klass = &tlBufferClass;
    buf->buf = tlbuffer_new();
    return buf;
}
tlHotelBuffer* tlHotelBufferCast(tlValue v) {
    if (tlref_is(v) && tl_head(v)->klass->act == _BufferAct) {
        return (tlHotelBuffer*)v;
    }
    return null;
}

INTERNAL tlPause* _buffer_read(tlTask* task, tlArgs* args) {
    tlHotelBuffer* buffer = tlHotelBufferCast(args->target);
    assert(buffer);
    tlBuffer* buf = buffer->buf;
    assert(buf);

    print("canread: %d", canread(buf));
    char* data = malloc(canread(buf) + 1);
    int last = tlbuffer_read(buf, data, canread(buf));
    data[last] = 0;
    print("JUST READ: %s", data);
    TL_RETURN(tltext_from_take(task, data));
}

INTERNAL tlPause* _buffer_write(tlTask* task, tlArgs* args) {
    tlHotelBuffer* buffer = tlHotelBufferCast(args->target);
    assert(buffer);
    tlBuffer* buf = buffer->buf;
    assert(buf);

    tlText* text = tltext_cast(tlargs_get(args, 0));
    if (!text) TL_THROW("expected a Text");
    print("GOING TO WRITE: %s", tltext_data(text));
    TL_RETURN(tlINT(tlbuffer_write(buf, tltext_data(text), tltext_size(text))));
}

INTERNAL tlPause* _BufferAct(tlTask* task, tlArgs* args) {
    tlSym msg = args->msg;
    assert(msg);
    if (msg == s_read) return _buffer_read(task, args);
    if (msg == s_write) return _buffer_write(task, args);
    return null;
}

tlPause* _Buffer_new(tlTask* task, tlArgs* args) {
    TL_RETURN(tlBufferNew());
}

static tlClass tlBufferClass = {
    .name = "Buffer",
    .map = null,
    .send = tlActorReceive,
    .act = _BufferAct,
};

static void io_init() {
    s_read = tlSYM("read");
    s_write = tlSYM("write");
}

