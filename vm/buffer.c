// a locked buffer, usable for io
// a bytebuffer that where you can read/write from/to

#include "trace-on.h"

#define INIT_SIZE 128
#define MAX_SIZE_INCREMENT (8*1024)

struct tlBuffer {
    tlLock lock;
    char* data;
    int size;
    int readpos;
    int writepos;
};
static tlKind _tlBufferKind = {
    .name = "Buffer",
    .locked = true,
};
tlKind* tlBufferKind = &_tlBufferKind;

#define check(buf) assert(buf->readpos <= buf->writepos && buf->writepos <= buf->size)

#define readbuf(buf) ((const char*) (buf->data + buf->readpos))
#define writebuf(buf) (buf->data + buf->writepos)
#define didread(buf, len) (buf->readpos += len)
#define didwrite(buf, len) (buf->writepos += len)
#define canread(buf) (buf->writepos - buf->readpos)
#define canwrite(buf) (buf->size - buf->writepos)

tlBuffer* tlBufferNew() {
    tlBuffer* buf = tlAlloc(tlBufferKind, sizeof(tlBuffer));
    buf->data = malloc_atomic(INIT_SIZE);
    buf->size = INIT_SIZE;
    check(buf);

    assert(buf->data);
    trace("size: %d", buf->size);
    return buf;
}

INTERNAL void tlBufferFinalizer(tlBuffer* buf) {
    assert(tlBufferIs(buf));
    check(buf);
    free(buf->data);
}

char* tlBufferTakeData(tlBuffer* buf) {
    char* data = buf->data;
    buf->data = null;
    buf->size = 0;

    trace("size: %d", buf->size);
    return data;
}

int tlBufferSize(tlBuffer* buf) {
    return canread(buf);
}

// on every write, we compact the buffer and ensure there is enough space to write, growing if needed
INTERNAL void tlBufferCompact(tlBuffer* buf) {
    int len = canread(buf);
    trace("len: %d", len);
    if (len > 0) memmove(buf->data, buf->data + buf->readpos, len);
    buf->readpos = 0;
    buf->writepos = len;
    check(buf);
}
INTERNAL void tlBufferBeforeWrite(tlBuffer* buf, int len) {
    assert(tlBufferIs(buf));
    assert(len >= 0 && len < 100 * 1024 * 1024);

    tlBufferCompact(buf);
    while (canwrite(buf) < len) buf->size += max(buf->size * 2, MAX_SIZE_INCREMENT);
    trace("new size: %d", buf->size);
    buf->data = realloc(buf->data, buf->size);
    check(buf);
}

// rewind all or part of bytes just read, notice you cannot rewind after doing any kind of write
int tlBufferRewind(tlBuffer* buf, int len) {
    assert(tlBufferIs(buf));
    if (len < 0) {
        len = buf->readpos;
        buf->readpos = 0;
        check(buf);
        return len;
    }
    if (len > buf->readpos) len = buf->readpos;
    buf->readpos -= len;
    check(buf);
    return len;
}

// clear out the buffer
void tlBufferClear(tlBuffer* buf) {
    buf->readpos = 0; buf->writepos = 0; check(buf);
}

//const char * tlbuf_readbuf(tl_buf* buf) { return readbuf(buf); }

int tlBufferRead(tlBuffer* buf, char* to, int count) {
    assert(tlBufferIs(buf));
    assert(to);
    int len = canread(buf);
    if (len > count) len = count;
    memcpy(to, readbuf(buf), len);
    buf->readpos += len;
    check(buf);
    return len;
}
int tlBufferWrite(tlBuffer* buf, const char* from, int count) {
    assert(from);
    tlBufferBeforeWrite(buf, count);
    assert(canwrite(buf) >= count);
    memcpy(writebuf(buf), from, count);
    buf->writepos += count;
    check(buf);
    return count;
}
int tlBufferWriteByte(tlBuffer* buf, const char b) {
    return tlBufferWrite(buf, &b, 1);
}
int tlBufferReadSkip(tlBuffer* buf, int count) {
    assert(tlBufferIs(buf));
    int len = canread(buf);
    if (len > count) len = count;
    buf->readpos += len;
    check(buf);
    return len;
}
uint8_t tlBufferReadByte(tlBuffer* buf) {
    assert(tlBufferIs(buf));
    if (canread(buf) == 0) return 0;
    uint8_t r = *((uint8_t*)(buf->data + buf->readpos));
    buf->readpos += 1;
    return r;
}
uint32_t tlBufferReadUint24(tlBuffer* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 3; i++) {
        r = (r << 8) | tlBufferReadByte(buf);
    }
    return r;
}
uint32_t tlBufferReadUint32(tlBuffer* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        r = (r << 8) | tlBufferReadByte(buf);
    }
    return r;
}

int tlBufferFind(tlBuffer* buf, const char* text, int len) {
    const char* begin = readbuf(buf);
    char* at = strnstr(begin, text, max(canread(buf), len));
    if (!at) return -1;
    return at - begin;
}

// TODO check file size and check if we read all in end
// TODO if in a tlTask context, throw error?
tlBuffer* tlBufferFromFile(const char* file) {
    trace("file: %s", file);
    tlBuffer* buf = tlBufferNew();

    int fd = open(file, O_RDONLY, 0);
    if (fd < 0) {
        warning("cannot open: %s, %s", file, strerror(errno));
        return 0;
    }

    int len;
    while ((len = read(fd, writebuf(buf), canwrite(buf))) > 0) {
        didwrite(buf, len);
        tlBufferBeforeWrite(buf, 5 * 1024);
    }
    check(buf);

    close(fd);
    return buf;
}

INTERNAL tlHandle _buffer_size(tlArgs* args) {
    return tlINT(canread(tlBufferAs(tlArgsTarget(args))));
}
INTERNAL tlHandle _buffer_read(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));
    if (!buf) TL_THROW("expected a Buffer");

    trace("canread: %d", canread(buf));
    char* data = malloc_atomic(canread(buf) + 1);
    int last = tlBufferRead(buf, data, canread(buf));
    data[last] = 0;
    return tlTextFromTake(data, last);
}

INTERNAL tlHandle _buffer_write(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));
    if (!buf) TL_THROW("expected a Buffer");

    tlHandle v = tlArgsGet(args, 0);
    if (tlTextIs(v)) {
        tlText* text = tlTextAs(v);
        return tlINT(tlBufferWrite(buf, tlTextData(text), tlTextSize(text)));
    } else if (tlBufferIs(v)) {
        tlBuffer* from = tlBufferAs(v);
        int size = canread(from);
        tlBufferWrite(buf, readbuf(from), size);
        didread(from, size);
        return tlINT(size);
    } else {
        TL_THROW("expected a Text or Buffer or Bin to write");
    }
}
INTERNAL tlHandle _buffer_writeByte(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));
    if (!buf) TL_THROW("expected a Buffer");

    int b = tl_int_or(tlArgsGet(args, 0), 0);
    tlBufferWriteByte(buf, (uint8_t)b);
    return tlOne;
}

INTERNAL tlHandle _buffer_find(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));
    if (!buf) TL_THROW("expected a Buffer");

    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    int i = tlBufferFind(buf, tlTextData(text), tlTextSize(text));
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
            "size", _buffer_size,
            "read", _buffer_read,
            "write", _buffer_write,
            "writeByte", _buffer_writeByte,
            "find", _buffer_find,
            null
    );
}

static void buffer_init_vm(tlVm* vm) {
    tlMap* BufferStatic = tlClassMapFrom(
        "new", _Buffer_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Buffer"), BufferStatic);
}

