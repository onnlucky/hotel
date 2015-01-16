#include <tl.h>
#include <zlib.h>

static tlHandle _deflate(tlTask* task, tlArgs* args) {
    tlBin* in = tlBinCast(tlArgsGet(args, 0));
    if (!in) TL_THROW("require input binary as arg[1]");
    int level = tl_int_or(tlArgsGet(args, 1), Z_DEFAULT_COMPRESSION);
    if (level < 0) level = Z_DEFAULT_COMPRESSION;
    if (level > Z_BEST_COMPRESSION) level = Z_BEST_COMPRESSION;
    bool gzip = tl_bool(tlArgsGet(args, 2));

    z_stream strm;
    strm.zalloc = null;
    strm.zfree = null;
    strm.avail_in = tlBinSize(in);
    strm.next_in = (unsigned char*)tlBinData(in);
    strm.avail_out = 0;
    strm.next_out = null;
    if (gzip) {
        deflateInit2(&strm, level, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    } else {
        deflateInit(&strm, level);
    }

    tlBuffer* out = tlBufferNew();
    do {
        strm.next_out = (unsigned char*)tlBufferWriteData(out, 4 * 1024);
        int canw = strm.avail_out = tlBufferCanWrite(out);
        deflate(&strm, Z_FINISH);
        tlBufferDidWrite(out, canw - strm.avail_out);
    } while (strm.avail_in > 0);
    return tlBinFromBufferTake(out);
}

static tlHandle _inflate(tlTask* task, tlArgs* args) {
    tlBin* in = tlBinCast(tlArgsGet(args, 0));
    if (!in) TL_THROW("require input binary as arg[1]");
    bool gzip = tl_bool(tlArgsGet(args, 1));

    z_stream strm;
    strm.zalloc = null;
    strm.zfree = null;
    strm.avail_in = tlBinSize(in);
    strm.next_in = (unsigned char*)tlBinData(in);
    strm.avail_out = 0;
    strm.next_out = null;

    if (gzip) {
        inflateInit2(&strm, 16 + MAX_WBITS);
    } else {
        inflateInit(&strm);
    }

    tlBuffer* out = tlBufferNew();
    do {
        strm.next_out = (unsigned char*)tlBufferWriteData(out, 4 * 1024);
        int canw = strm.avail_out = tlBufferCanWrite(out);
        int r = inflate(&strm, Z_NO_FLUSH);
        tlBufferDidWrite(out, canw - strm.avail_out);
        switch (r) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
                (void)inflateEnd(&strm);
                TL_THROW("unable to inflate: invalid data (%d)", r);
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                TL_THROW("unable to deflate: not enough memory");
        }
        if (r == Z_STREAM_END) {
            inflateEnd(&strm);
            return tlBinFromBufferTake(out);
        }
    } while (strm.avail_in > 0);

    inflateEnd(&strm);
    TL_THROW("unable to inflate: premature end of data");
}

tlHandle tl_load() {
    return tlClassObjectFrom("deflate", _deflate, "inflate", _inflate, null);
}

