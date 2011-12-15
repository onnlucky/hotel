// a bytebuffer that where you can read/write from/to

#include "buf.h"
#include "trace-off.h"

#define SIZE (10*1024)

struct tl_buf {
    char *data;
    int size;
    int readpos;
    int writepos;
    bool autogrow;
};

#define check(buf) assert(buf->readpos <= buf->writepos && buf->writepos <= buf->size)

tl_buf* tlbuf_new() {
    tl_buf* buf = calloc(1, sizeof(tl_buf));
    buf->data = malloc(SIZE);
    buf->size = SIZE;
    buf->autogrow = true;

    check(buf);
    trace("size: %d", buf->size);
    return buf;
}

void tlbuf_free(tl_buf* buf) {
    check(buf);
    free(buf->data);
    free(buf);
}

char* tlbuf_free_get(tl_buf* buf) {
    check(buf);
    char* data = buf->data;
    free(buf);
    return data;
}

int tlbuf_size(tl_buf* buf) {
    return buf->size;
}

int tlbuf_grow(tl_buf* buf) {
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

void tlbuf_autogrow(tl_buf* buf) {
    if (canread(buf) < 1024) tlbuf_grow(buf);
    assert(canwrite(buf) >= 1024);
}

void tlbuf_reread(tl_buf* buf) { buf->readpos = 0; check(buf); }
void tlbuf_clear(tl_buf* buf) { buf->readpos = 0; buf->writepos = 0; check(buf); }
int tlbuf_canread(const tl_buf* buf) { return canread(buf); }
int tlbuf_canwrite(const tl_buf* buf) { return canwrite(buf); }
int tlbuf_readpos(const tl_buf* buf) { return buf->readpos; }
int tlbuf_writepos(const tl_buf* buf) { return buf->writepos; }
const char * tlbuf_readbuf(tl_buf* buf) { return readbuf(buf); }

void tlbuf_compact(tl_buf* buf) {
    int len = canread(buf);
    trace("len: %d", len);
    if (len > 0) {
        memcpy(buf->data, buf->data + buf->readpos, len);
    }
    buf->readpos = 0;
    buf->writepos = len;
    check(buf);
}
int tlbuf_read(tl_buf* buf, char* to, int count) {
    int len = canread(buf);
    if (len > count) len = count;
    memcpy(to, readbuf(buf), len);
    buf->readpos += len;
    check(buf);
    return len;
}
int tlbuf_write(tl_buf* buf, const char* from, int count) {
    int len = canwrite(buf);
    if (len > count) len = count;
    memcpy(writebuf(buf), from, len);
    buf->writepos += len;
    check(buf);
    return len;
}
int tlbuf_write_uint8(tl_buf* buf, const char b) {
    return tlbuf_write(buf, &b, 1);
}
int tlbuf_read_skip(tl_buf* buf, int count) {
    int len = canread(buf);
    if (len > count) len = count;
    buf->readpos += len;
    check(buf);
    return len;
}
uint8_t tlbuf_read_uint8(tl_buf* buf) {
    uint8_t r = *((uint8_t*)(buf->data + buf->readpos));
    buf->readpos += 1;
    return r;
}
uint32_t tlbuf_read_uint24(tl_buf* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 3; i++) {
        r = (r << 8) | tlbuf_read_uint8(buf);
    }
    return r;
}
uint32_t tlbuf_read_uint32(tl_buf* buf) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        r = (r << 8) | tlbuf_read_uint8(buf);
    }
    return r;
}

int tlbuf_find(tl_buf* buf, const char* text, int len) {
    const char* begin = readbuf(buf);
    char* at = strnstr(begin, text, max(canread(buf), len));
    if (!at) return -1;
    return at - begin;
}

// TODO check file size and check if we read all in end
tl_buf* tlbuf_new_from_file(const char* file) {
    trace("file: %s", file);
    tl_buf* buf = tlbuf_new();

    int fd = open(file, O_RDONLY, 0);
    if (fd < 0) {
        warning("cannot open: %s, %s", file, strerror(errno));
        return 0;
    }

    int len;
    while ((len = read(fd, writebuf(buf), canwrite(buf))) > 0) {
        didwrite(buf, len);
        tlbuf_autogrow(buf);
    }
    check(buf);

    close(fd);
    return buf;
}


