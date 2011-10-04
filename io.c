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
    TL_RETURN(tlTextNewTake(task, data));
}

INTERNAL tlPause* _buffer_write(tlTask* task, tlArgs* args) {
    tlHotelBuffer* buffer = tlHotelBufferCast(args->target);
    assert(buffer);
    tlBuffer* buf = buffer->buf;
    assert(buf);

    tlText* text = tlTextCast(tlargs_get(args, 0));
    if (!text) TL_THROW("expected a Text");
    TL_RETURN(tlINT(tlbuffer_write(buf, tlTextData(text), tlTextSize(text))));
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

/*
INTERNAL tlPause* _io_open(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsAt(args, 0));
    if (!name) TL_THROW("expected a Text as file name");
    bool err = false;
    int flags = tlIntCast(tlArgsAt(args, 1), &err);
    if (err) TL_THROW("expected a Int as flags");
    int perms = tlIntCast(tlArgsAt(args, 2), &err);
    if (err) TL_THROW("expected a Int as perm");

    int fd = open(tlTextData(name), flags, perms);
    if (fd < 0) TL_THROW("error opening file: %s", tlTextData(name));
    return TL_RETURN(tlINT(fd));
}

INTERNAL tlPause* _io_read2(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsTarget(args));
    tlBuffer* buf = tlBufferCast(tlArgsAt(args, 0));
    assert(file->owner == task);
    assert(buf->owner == task);

    int len = tlBufferReadFd(buf, fd);
    tlActorRelease(task, buf);
    TL_RETURN(len);
}

INTERNAL tlPause* _io_read(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("expected a Buffer");
    return tlActorGet(task, buf, _io_read2);
}
*/

static tlClass tlBufferClass = {
    .name = "Buffer",
    .send = tlActorReceive,
    .act = _BufferAct,
};

static void io_init() {
    s_read = tlSYM("read");
    s_write = tlSYM("write");
}

