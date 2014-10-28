// a locked buffer, usable for io
// a bytebuffer that where you can read/write from/to

#include "trace-off.h"

static int at_offset_min(tlHandle v, int size);
static int at_offset_max(tlHandle v, int size);

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
tlKind* tlBufferKind;

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

int tlBufferSize(tlBuffer* buf) {
    return canread(buf);
}
const char* tlBufferData(tlBuffer* buf) {
    return readbuf(buf);
}
int tlBufferGet(tlBuffer* buf, int at) {
    if (at < 0 || at >= canread(buf)) return -1;
    return 0xFF & buf->data[buf->readpos + at];
}

// on every write, we compact the buffer and ensure there is enough space to write, growing if needed
INTERNAL void tlBufferCompact(tlBuffer* buf) {
    if (buf->readpos == 0) return;
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
int tlBufferCanWrite(tlBuffer* buf) {
    return canwrite(buf);
}
// get a pointer to the direct data underlying this buffer, guaranteed to have at least #len bytes of space
char* tlBufferWriteData(tlBuffer* buf, int len) {
    tlBufferBeforeWrite(buf, len);
    return writebuf(buf);
}
void tlBufferDidWrite(tlBuffer* buf, int len) {
    didwrite(buf, len);
}

char* tlBufferTakeData(tlBuffer* buf) {
    tlBufferCompact(buf);
    char* data = buf->data;
    buf->data = null;
    buf->size = 0;
    buf->readpos = buf->writepos = 0;

    trace("size: %d", buf->size);
    return data;
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
uint8_t tlBufferPeekByte(tlBuffer* buf, int at) {
    assert(tlBufferIs(buf));
    if (canread(buf) <= at) return 0;
    uint8_t r = *((uint8_t*)(buf->data + buf->readpos + at));
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

/// object Buffer: a mutable object containing bytes to which you can write and read from
/// while reading the buffer temporarily keeps the read bytes until any write operation

/// size: return the current amount of bytes that can be read
INTERNAL tlHandle _buffer_size(tlTask* task, tlArgs* args) {
    return tlINT(canread(tlBufferAs(tlArgsTarget(args))));
}

/// compact: discard any temporarily bytes already read
INTERNAL tlHandle _buffer_compact(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    tlBufferCompact(buf);
    return buf;
}
/// clear: reset the buffer, discarding any read and unread bytes
INTERNAL tlHandle _buffer_clear(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    tlBufferClear(buf);
    return buf;
}
/// rewind: undo the read operations by rewinding the read position back to the bytes already read
INTERNAL tlHandle _buffer_rewind(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    tlBufferRewind(buf, -1);
    return buf;
}
/// read(max): read #max bytes from the buffer returning them as a #Bin (binary list)
/// if no #max is not given, read all bytes from the buffer
INTERNAL tlHandle _buffer_read(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));

    int max = canread(buf);
    int len = tl_int_or(tlArgsGet(args, 0), max);
    if (len > max) len = max;

    trace("canread: %d", len);
    const char* from = readbuf(buf);
    didread(buf, len);
    return tlBinFromCopy(from, len);
}

static int process_utf8(const char* from, int len, char** into, int* intolen, int* intochars);
INTERNAL tlHandle _buffer_readString(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));

    int max = canread(buf);
    int len = tl_int_or(tlArgsGet(args, 0), max);
    if (len > max) len = max;
    trace("canread: %d", len);

    const char* from = readbuf(buf);
    char* into = malloc_atomic(len + 1);

    int written = 0; int chars = 0;
    int read = process_utf8(from, len, &into, &written, &chars);
    assert(read <= len);
    trace("read: %d", read);

    didread(buf, read);
    return tlStringFromTake(into, written);
}

/// readByte: read a single byte from the buffer, returns a number between 0-255
INTERNAL tlHandle _buffer_readByte(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
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
    if (tlBinIs(v)) {
        tlBin* bin = tlBinAs(v);
        return tlBufferWrite(buf, tlBinData(bin), tlBinSize(bin));
    }
    if (tlStringIs(v)) {
        tlString* str = tlStringAs(v);
        return tlBufferWrite(buf, tlStringData(str), tlStringSize(str));
    }
    if (tlBufferIs(v)) {
        tlBuffer* from = tlBufferAs(v);
        int size = tlBufferSize(from);
        tlBufferWrite(buf, tlBufferData(from), size);
        didread(from, size);
        return size;
    }
    if (error) *error = "expected a String or Buffer or Bin to write";
    return 0;
}

/// write(bytes*): write all passed in arguments as bytes
/// bytes can be #String's or #Bin's or other #Buffer's or numbers
/// Numbers are interpreted as a single byte by masking with `0xFF`.
INTERNAL tlHandle _buffer_write(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));

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

/// find(bytes): find the position of the bytes passed in, or null if no matching sequence is found
/// [from] start here instead of beginning
/// [upto] search no further than this posision
INTERNAL tlHandle _buffer_find(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    int size = tlBufferSize(buf);
    assert(size == canread(buf));

    tlHandle arg0 = tlArgsGet(args, 0);
    if (!arg0) TL_THROW("expected a String or Number");

    int at = 1;
    tlHandle afrom = tlArgsMapGet(args, tlSYM("from"));
    if (!afrom) afrom = tlArgsGet(args, at++);
    int from = at_offset_min(afrom, tlBufferSize(buf));
    if (from < 0) TL_THROW("from must be Number, not: %s", tl_str(afrom));

    tlHandle aupto = tlArgsMapGet(args, tlSYM("upto"));
    if (!aupto) aupto = tlArgsGet(args, at++);
    int upto = at_offset_max(aupto, tlBufferSize(buf));
    if (upto < 0) TL_THROW("upto must be Number, not: %s", tl_str(aupto));

    if (from >= upto) return tlNull;
    assert(from >= 0 && from <= canread(buf));
    assert(upto >= 0 && upto <= canread(buf));

    //bool backward = tl_bool(tlArgsMapGet(args, tlSYM("backward")));

    const char* needle = null;
    int needle_len = 0;
    if (tlStringIs(arg0)) { needle = tlStringData(arg0); needle_len = tlStringSize(arg0); }
    else if (tlBinIs(arg0)) { needle = tlBinData(arg0); needle_len = tlBinSize(arg0); }
    else if (tlBufferIs(arg0)) { needle = tlBufferData(arg0); needle_len = tlBufferSize(arg0); } // TODO check for locked buffer?!
    if (needle) {
        const char* begin = readbuf(buf);
        char* at = memmem(begin + from, upto - from, needle, needle_len);
        if (!at) return tlNull;
        return tlINT(1 + at - begin);
    }

    if (tlNumberIs(arg0) || tlCharIs(arg0)) {
        if (size == 0) return tlNull;
        int b = tl_int(arg0) & 0xFF;
        const char* data = readbuf(buf);
        int at = from;
        for (; at < upto; at++) if (data[at] == b) return tlINT(1 + at);
        return tlNull;
    }
    TL_THROW("expected a String or Number");
}

/// startsWith(bytes): returns true if buffer starts with this sequence of bytes
INTERNAL tlHandle _buffer_startsWith(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));

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

INTERNAL tlHandle _buffer_dump(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    for (int i = 0; i < tlBufferSize(buf); i++) {
        if (i > 0 && i % 64 == 0) fprintf(stdout, "\n");
        int c = tlBufferGet(buf, i);
        if (c < 32 || c >= 127) {
            fprintf(stdout, ".");
        } else {
            fprintf(stdout, "%c", c);
        }
    }
    printf("\n");
    return tlNull;
}
INTERNAL tlHandle _buffer_hexdump(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferAs(tlArgsTarget(args));
    for (int i = 0; i < tlBufferSize(buf); i++) {
        if (i > 0 && i % 16 == 0) fprintf(stdout, "\n");
        else if (i > 0 && i % 2 == 0) fprintf(stdout, " ");
        int c = tlBufferGet(buf, i);
        fprintf(stdout, "%02x", c);
    }
    fprintf(stdout, "\n");
    return tlNull;
}

/// new(bytes*): create a new buffer filled with bytes
INTERNAL tlHandle _Buffer_new(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferNew();

    const char* error = null;
    for (int i = 0;; i++) {
        tlHandle v = tlArgsGet(args, i);
        if (!v) return buf;
        buffer_write_object(buf, v, &error);
        if (error) TL_THROW("%s", error);
    }
}

static void buffer_init() {
    INIT_KIND(tlBufferKind);
    tlBufferKind->klass = tlClassObjectFrom(
            "size", _buffer_size,
            "compact", _buffer_compact,
            "clear", _buffer_clear,
            "rewind", _buffer_rewind,
            "read", _buffer_read,
            "readString", _buffer_readString,
            "readByte", _buffer_readByte,
            "find", _buffer_find,
            "write", _buffer_write,
            "startsWith", _buffer_startsWith,
            "dump", _buffer_dump,
            "hexdump", _buffer_hexdump,
            null
    );
}

static void buffer_init_vm(tlVm* vm) {
    tlObject* BufferStatic = tlClassObjectFrom(
        "new", _Buffer_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Buffer"), BufferStatic);
}

