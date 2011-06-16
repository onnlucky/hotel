// a bytebuffer that where you can read/write from/to

#include "trace-off.h"

#define SIZE (10*1024)

typedef struct tBuffer tBuffer;
struct tBuffer {
    char *data;
    int size;
    int readpos;
    int writepos;
};

#define check(buf) assert(buf->readpos <= buf->writepos && buf->writepos <= buf->size)

tBuffer* tbuffer_new() {
    tBuffer* buf = calloc(1, sizeof(tBuffer));
    buf->data = malloc(SIZE);
    buf->size = SIZE;

    check(buf);
    trace("size: %d", buf->size);
    return buf;
}

void tbuffer_free(tBuffer* buf) {
    check(buf);
    free(buf->data);
    free(buf);
}

char* tbuffer_free_get(tBuffer* buf) {
    check(buf);
    char* data = buf->data;
    free(buf);
    return data;
}

int tbuffer_size(tBuffer* buf) {
    return buf->size;
}

int tbuffer_grow(tBuffer* buf) {
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

void tbuffer_reread(tBuffer* buf) { buf->readpos = 0; check(buf); }
void tbuffer_clear(tBuffer* buf) { buf->readpos = 0; buf->writepos = 0; check(buf); }
int tbuffer_canread(const tBuffer* buf) { return canread(buf); }
int tbuffer_canwrite(const tBuffer* buf) { return canwrite(buf); }
int tbuffer_readpos(const tBuffer* buf) { return buf->readpos; }
int tbuffer_writepos(const tBuffer* buf) { return buf->writepos; }
const char * tbuffer_readbuf(tBuffer* buf) { return readbuf(buf); }

void tbuffer_compact(tBuffer* buf) {
    int len = canread(buf);
    trace("len: %d", len);
    if (len > 0) {
        memcpy(buf->data, buf->data + buf->readpos, len);
    }
    buf->readpos = 0;
    buf->writepos = len;
    check(buf);
}
int tbuffer_read(tBuffer* buf, char* to, int count) {
    int len = canread(buf);
    if (len > count) len = count;
    memcpy(to, readbuf(buf), len);
    buf->readpos += len;
    check(buf);
    return len;
}
int tbuffer_write(tBuffer* buf, const char* from, int count) {
    int len = canwrite(buf);
    if (len > count) len = count;
    memcpy(writebuf(buf), from, len);
    buf->writepos += len;
    check(buf);
    return len;
}
int tbuffer_write_uint8(tBuffer* buf, const char b) {
    return tbuffer_write(buf, &b, 1);
}
int tbuffer_read_skip(tBuffer* buf, int count) {
    int len = canread(buf);
    if (len > count) len = count;
    buf->readpos += len;
    check(buf);
    return len;
}
uint8_t tbuffer_read_uint8(tBuffer* buf) {
    uint8_t r = *((uint8_t*)(buf->data + buf->readpos));
    buf->readpos += 1;
    return r;
}
uint32_t tbuffer_read_uint24(tBuffer* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 3; i++) {
        r = (r << 8) | tbuffer_read_uint8(buf);
    }
    return r;
}
uint32_t tbuffer_read_uint32(tBuffer* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        r = (r << 8) | tbuffer_read_uint8(buf);
    }
    return r;
}

// TODO check file size and check if we read all in end
static tBuffer* tbuffer_new_from_file(const char* file) {
    trace("file: %s", file);
    tBuffer* buf = tbuffer_new();

    int fd = open(file, O_RDONLY, 0);
    if (fd < 0) {
        warning("cannot open: %s, %s", file, strerror(errno));
        return 0;
    }

    int len;
    while ((len = read(fd, writebuf(buf), canwrite(buf))) > 0) {
        didwrite(buf, len);
        if (canwrite(buf) < 1024) tbuffer_grow(buf);
        assert(canwrite(buf) >= 1024);
    }
    check(buf);

    close(fd);
    return buf;
}
