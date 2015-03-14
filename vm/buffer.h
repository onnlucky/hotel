#ifndef _buffer_h_
#define _buffer_h_

#include "tl.h"

struct tlBuffer {
    tlLock lock;
    char* data;
    int size;
    int readpos;
    int writepos;
};

int tlBufferRewind(tlBuffer* buf, int len);
int tlBufferWriteRewind(tlBuffer* buf, int len);

void tlBufferSkipByte(tlBuffer* buf);
uint8_t tlBufferPeekByte(tlBuffer* buf, int at);

char* tlBufferTakeData(tlBuffer* buf);
char* tlBufferWriteData(tlBuffer* buf, int len);
void tlBufferDidWrite(tlBuffer* buf, int len);

void tlBufferBeforeWrite(tlBuffer* buf, int len);
#define check(buf) assert(buf->readpos <= buf->writepos && buf->writepos <= buf->size)

#define readbuf(buf) ((const char*) (buf->data + buf->readpos))
#define writebuf(buf) (buf->data + buf->writepos)
#define didread(buf, len) (buf->readpos += len)
#define didwrite(buf, len) (buf->writepos += len)
#define canread(buf) (buf->writepos - buf->readpos)
#define canwrite(buf) (buf->size - buf->writepos)

int buffer_write_object(tlBuffer* buf, tlHandle v, const char** error);

void buffer_init();
void buffer_init_vm(tlVm* vm);

#endif
