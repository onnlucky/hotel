// a locked buffer, usable for io
// a bytebuffer that where you can read/write from/to

#include "trace-off.h"

static int at_offset(tlHandle v, int size);

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
    buf->readpos = buf->writepos = 0;

    trace("size: %d", buf->size);
    return data;
}

int tlBufferSize(tlBuffer* buf) {
    return canread(buf);
}
const char* tlBufferData(tlBuffer* buf) {
    return readbuf(buf);
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
    while (canwrite(buf) < len) buf->size += min(buf->size * 2, MAX_SIZE_INCREMENT);
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
int tlBufferWriteRewind(tlBuffer* buf, int len) {
    assert(tlBufferIs(buf));
    assert(len >= 0);
    if (len > canread(buf)) len = canread(buf);
    buf->writepos -= len;
    check(buf);
    return len;
}
int tlBufferReadSkip(tlBuffer* buf, int count) {
    assert(tlBufferIs(buf));
    int len = canread(buf);
    if (len > count) len = count;
    buf->readpos += len;
    check(buf);
    return len;
}
void tlBufferSkipByte(tlBuffer* buf) {
    assert(tlBufferIs(buf));
    if (canread(buf) == 0) return;
    buf->readpos += 1;
}
uint8_t tlBufferPeekByte(tlBuffer* buf) {
    assert(tlBufferIs(buf));
    if (canread(buf) == 0) return 0;
    uint8_t r = *((uint8_t*)(buf->data + buf->readpos));
    return r;
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

int tlBufferFind(tlBuffer* buf, const char* str, int len) {
    const char* begin = readbuf(buf);
    char* at = strnstr(begin, str, max(canread(buf), len));
    if (!at) return -1;
    return at - begin;
}

// TODO check file size and check if we read all in end
// TODO if in a tlTask constr, throw error?
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
INTERNAL tlHandle _buffer_compact(tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    tlBufferCompact(buf);
    return buf;
}
INTERNAL tlHandle _buffer_clear(tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    tlBufferClear(buf);
    return buf;
}
INTERNAL tlHandle _buffer_rewind(tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    tlBufferRewind(buf, -1);
    return buf;
}
// TODO this should return bytes, not strings ...
INTERNAL tlHandle _buffer_read(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));

    int max = canread(buf);
    int len = tl_int_or(tlArgsGet(args, 0), max);
    if (len > max) len = max;

    trace("canread: %d", len);
    char* data = malloc_atomic(len + 1);
    int last = tlBufferRead(buf, data, len);
    data[last] = 0;
    return tlStringFromTake(data, last);
}
INTERNAL tlHandle _buffer_readByte(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));
    if (canread(buf) == 0) return tlNull;
    return tlINT(tlBufferReadByte(buf));
}

INTERNAL int buffer_write_object(tlBuffer* buf, tlHandle v, const char** error) {
    if (tlIntIs(v)) {
        return tlBufferWriteByte(buf, tl_int(v));
    }
    if (tlNumberIs(v)) {
        return tlBufferWriteByte(buf, (int)tl_double(v));
    }
    if (tlStringIs(v)) {
        tlString* str = tlStringAs(v);
        return tlBufferWrite(buf, tlStringData(str), tlStringSize(str));
    }
    if (tlBufferIs(v)) {
        tlBuffer* from = tlBufferAs(v);
        int size = canread(from);
        tlBufferWrite(buf, readbuf(from), size);
        didread(from, size);
        return size;
    }
    if (error) *error = "expected a String or Buffer or Bin to write";
    return 0;
}

INTERNAL tlHandle _buffer_write(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));
    if (!buf) TL_THROW("expected a Buffer");

    int written = 0;
    const char* error = null;
    for (int i = 0;; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) return tlINT(written);
        written += buffer_write_object(buf, v, &error);
        if (error) TL_THROW("%s", error);
    }
    return tlNull;
}

INTERNAL tlHandle _buffer_find(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));

    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    int from = max(0, tl_int_or(tlArgsGet(args, 1), 1) - 1);
    if (from < 0) return tlNull;

    int upto = min(tlBufferSize(buf), tl_int_or(tlArgsGet(args, 2), tlBufferSize(buf)));
    if (from >= upto) return tlNull;

    assert(tlBufferSize(buf) == canread(buf));
    const char* begin = readbuf(buf);
    char* at = strnstr(begin + from, tlStringData(str), max(upto - from, tlStringSize(str)));
    if (!at) return tlNull;
    return tlINT(1 + at - begin);
}
INTERNAL tlHandle _buffer_findByte(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));

    int b = tl_int_or(tlArgsGet(args, 0), -1);
    if (b == -1) TL_THROW("expected a number (0 - 255)");

    int from = at_offset(tlArgsGet(args, 1), tlBufferSize(buf));
    if (from < 0) return tlNull;

    const char* data = readbuf(buf);
    int at = from;
    int end = canread(buf);
    for (; at < end; at++) if (data[at] == b) return tlINT(1 + at);
    return tlNull;
}
INTERNAL tlHandle _buffer_startsWith(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsTarget(args));

    tlString* start = tlStringCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a String");

    int from = at_offset(tlArgsGet(args, 1), tlBufferSize(buf));
    if (from < 0) return tlFalse;

    int bufsize = tlBufferSize(buf);
    int startsize = tlStringSize(start);
    if (bufsize + from < startsize) return tlFalse;

    int r = strncmp(tlBufferData(buf) + from, tlStringData(start), startsize);
    return tlBOOL(r == 0);
}

INTERNAL tlHandle _Buffer_new(tlArgs* args) {
    tlBuffer* buf = tlBufferNew();

    const char* error = null;
    for (int i = 0;; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) return buf;
        buffer_write_object(buf, v, &error);
        if (error) TL_THROW("%s", error);
    }
}
static tlHandle _isBuffer(tlArgs* args) {
    return tlBOOL(tlBufferIs(tlArgsGet(args, 0)));
}

static void buffer_init() {
    _tlBufferKind.klass = tlClassMapFrom(
            "size", _buffer_size,
            "compact", _buffer_compact,
            "clear", _buffer_clear,
            "rewind", _buffer_rewind,
            "read", _buffer_read,
            "readString", _buffer_read,
            "readByte", _buffer_readByte,
            "find", _buffer_find,
            "findByte", _buffer_findByte, // TODO remove
            "write", _buffer_write,
            "startsWith", _buffer_startsWith,
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

