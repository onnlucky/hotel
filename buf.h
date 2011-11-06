#ifndef _buf_h_
#define _buf_h_

typedef struct tl_buf tl_buf;

tl_buf* tlbuf_new_from_file(const char* file);
int tlbuf_write_uint8(tl_buf* buf, const char b);
char* tlbuf_free_get(tl_buf* buf);

#endif //_buf_h_
