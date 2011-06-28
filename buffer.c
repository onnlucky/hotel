// a bytebuffer that where you can read/write from/to

#include "trace-off.h"

#define SIZE (10*1024)

typedef struct tlBuffer tlBuffer;
struct tlBuffer {
    char *data;
    int size;
    int readpos;
    int writepos;
};

#define check(buf) assert(buf->readpos <= buf->writepos && buf->writepos <= buf->size)

tlBuffer* tlbuffer_new() {
    tlBuffer* buf = calloc(1, sizeof(tlBuffer));
    buf->data = malloc(SIZE);
    buf->size = SIZE;

    check(buf);
    trace("size: %d", buf->size);
    return buf;
}

void tlbuffer_free(tlBuffer* buf) {
    check(buf);
    free(buf->data);
    free(buf);
}

char* tlbuffer_free_get(tlBuffer* buf) {
    check(buf);
    char* data = buf->data;
    free(buf);
    return data;
}

int tlbuffer_size(tlBuffer* buf) {
    return buf->size;
}

int tlbuffer_grow(tlBuffer* buf) {
    buf->size += SIZE;
    trace("new size: %d", buf->size);
    buf->data = realloc(buf->data, buf->size);
    check(buf);
    return buf->size;
}

#define readbuf(buf) ((const char*) (buf->data + buf->readpos))
#define writebuf(buf) (buf->data + buf->writepos)
#define didread(buf, len) (buf->readpos += len)
#define didwrite(buf, len) (buf->writepos += len)
#define canread(buf) (buf->writepos - buf->readpos)
#define canwrite(buf) (buf->size - buf->writepos)

void tlbuffer_reread(tlBuffer* buf) { buf->readpos = 0; check(buf); }
void tlbuffer_clear(tlBuffer* buf) { buf->readpos = 0; buf->writepos = 0; check(buf); }
int tlbuffer_canread(const tlBuffer* buf) { return canread(buf); }
int tlbuffer_canwrite(const tlBuffer* buf) { return canwrite(buf); }
int tlbuffer_readpos(const tlBuffer* buf) { return buf->readpos; }
int tlbuffer_writepos(const tlBuffer* buf) { return buf->writepos; }
const char * tlbuffer_readbuf(tlBuffer* buf) { return readbuf(buf); }

void tlbuffer_compact(tlBuffer* buf) {
    int len = canread(buf);
    trace("len: %d", len);
    if (len > 0) {
        memcpy(buf->data, buf->data + buf->readpos, len);
    }
    buf->readpos = 0;
    buf->writepos = len;
    check(buf);
}
int tlbuffer_read(tlBuffer* buf, char* to, int count) {
    int len = canread(buf);
    if (len > count) len = count;
    memcpy(to, readbuf(buf), len);
    buf->readpos += len;
    check(buf);
    return len;
}
int tlbuffer_write(tlBuffer* buf, const char* from, int count) {
    int len = canwrite(buf);
    if (len > count) len = count;
    memcpy(writebuf(buf), from, len);
    buf->writepos += len;
    check(buf);
    return len;
}
int tlbuffer_write_uint8(tlBuffer* buf, const char b) {
    return tlbuffer_write(buf, &b, 1);
}
int tlbuffer_read_skip(tlBuffer* buf, int count) {
    int len = canread(buf);
    if (len > count) len = count;
    buf->readpos += len;
    check(buf);
    return len;
}
uint8_t tlbuffer_read_uint8(tlBuffer* buf) {
    uint8_t r = *((uint8_t*)(buf->data + buf->readpos));
    buf->readpos += 1;
    return r;
}
uint32_t tlbuffer_read_uint24(tlBuffer* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 3; i++) {
        r = (r << 8) | tlbuffer_read_uint8(buf);
    }
    return r;
}
uint32_t tlbuffer_read_uint32(tlBuffer* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        r = (r << 8) | tlbuffer_read_uint8(buf);
    }
    return r;
}

// TODO check file size and check if we read all in end
static tlBuffer* tlbuffer_new_from_file(const char* file) {
    trace("file: %s", file);
    tlBuffer* buf = tlbuffer_new();

    int fd = open(file, O_RDONLY, 0);
    if (fd < 0) {
        warning("cannot open: %s, %s", file, strerror(errno));
        return 0;
    }

    int len;
    while ((len = read(fd, writebuf(buf), canwrite(buf))) > 0) {
        didwrite(buf, len);
        if (canwrite(buf) < 1024) tlbuffer_grow(buf);
        assert(canwrite(buf) >= 1024);
    }
    check(buf);

    close(fd);
    return buf;
}
