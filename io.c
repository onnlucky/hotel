// hotel simple io

static tlSym s_read;
static tlSym s_write;

TL_REF_TYPE(Buffer);
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

INTERNAL tlPause* _BufferRead(tlTask* task, tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(args->target);
    assert(buffer);
    tl_buf* buf = buffer->buf;
    assert(buf);

    print("canread: %d", canread(buf));
    char* data = malloc(canread(buf) + 1);
    int last = tlbuf_read(buf, data, canread(buf));
    data[last] = 0;
    TL_RETURN(tlTextNewTake(task, data));
}

INTERNAL tlPause* _BufferWrite(tlTask* task, tlArgs* args) {
    tlBuffer* buffer = tlBufferCast(args->target);
    assert(buffer);
    tl_buf* buf = buffer->buf;
    assert(buf);

    tlText* text = tlTextCast(tlargs_get(args, 0));
    if (!text) TL_THROW("expected a Text");
    TL_RETURN(tlINT(tlbuf_write(buf, tlTextData(text), tlTextSize(text))));
}

tlPause* _Buffer_new(tlTask* task, tlArgs* args) {
    TL_RETURN(tlBufferNew(task));
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

static void io_init() {
    s_read = tlSYM("read");
    s_write = tlSYM("write");

    _tlBufferClass.map = tlClassMapFrom(
            "read", _BufferRead,
            "write", _BufferWrite,
            null
    );
}

