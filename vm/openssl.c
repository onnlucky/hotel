#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/md5.h>

#include "tl.h"

#include "trace-off.h"

#ifdef PLATFORM_MACOSX
#define SSLCONST
#else
#define SSLCONST const
#endif


static int sb_write(BIO* bio, const char* from, int len) {
    trace("write: %d", len);
    BIO_clear_retry_flags(bio);
    return tlBufferWrite(tlBufferAs(bio->ptr), from, len);
}
static int sb_read(BIO* bio, char* into, int len) {
    trace("read: %d", tlBufferSize(tlBufferAs(bio->ptr)));
    BIO_clear_retry_flags(bio);
    int res = tlBufferRead(tlBufferAs(bio->ptr), into, len);
    if (res == 0) {
        BIO_set_retry_read(bio);
        return -1;
    }
    return res;
}
static int sb_puts(BIO* bio, const char* str) {
    fatal("oeps");
    return 0;
}
static int sb_gets(BIO* bio, char* into, int len) {
    fatal("oeps");
    return 0;
}
static long sb_ctrl(BIO* bio, int cmd, long num, void* ptr) {
    trace("BUFFER CTRL: %d %ld %p", cmd, num, ptr);
    switch (cmd) {
        case BIO_CTRL_RESET: return 0;
        case BIO_CTRL_EOF: return 0;
        case BIO_CTRL_INFO: return 0;
        case BIO_CTRL_SET: fatal("set"); return 0;
        case BIO_CTRL_GET: fatal("get"); return 0;
        case BIO_CTRL_PUSH: return 0;
        case BIO_CTRL_POP: return 0;
        case BIO_CTRL_GET_CLOSE: return bio->shutdown;
        case BIO_CTRL_SET_CLOSE: return bio->shutdown = (int)num;
        case BIO_CTRL_PENDING: return tlBufferSize(tlBufferAs(bio->ptr)) > 0;
        case BIO_CTRL_FLUSH: return 1;
        case BIO_CTRL_DUP: return 1;
        case BIO_CTRL_WPENDING: return tlBufferSize(tlBufferAs(bio->ptr)) > 0;
        case BIO_CTRL_SET_CALLBACK: return 0;
        case BIO_CTRL_GET_CALLBACK: return 0;
        default: fatal("%d", cmd); return 0;
    }
    return 0;
}
static int sb_create(BIO* bio) {
    trace("create %p", bio);
    bio->init = 1;
    bio->num = -1;
    bio->flags = BIO_FLAGS_UPLINK;
    return 1;
}
static int sb_destroy(BIO* bio) {
    return 1;
}
static long sb_callback_ctrl(BIO* bio, int cmd, bio_info_cb* cb) {
    fatal("oeps");
    return 0;
}
BIO_METHOD buffer_bio_method = {
    .type = BIO_TYPE_FD,
    .name = "tl-buffer",
    .bwrite = sb_write,
    .bread = sb_read,
    .bputs = sb_puts,
    .bgets = sb_gets,
    .ctrl = sb_ctrl,
    .create = sb_create,
    .destroy = sb_destroy,
    .callback_ctrl = sb_callback_ctrl,
};
static tlBuffer* bio_buffer(BIO* bio) {
    return tlBufferAs(bio->ptr);
}

TL_REF_TYPE(tlSSL);
struct tlSSL {
    tlLock lock;
    BIO* rbio;
    BIO* wbio;
    SSL* session;
    tlBuffer* rbuf; // here for GC visibility
    tlBuffer* wbuf; // idem
};

// TODO server state, getting/setting keys and all that good stuff ...
INTERNAL tlHandle _SSL_new(tlArgs* args) {
    tlSSL* ssl = tlAlloc(tlSSLKind, sizeof(tlSSL));
    ssl->rbio = BIO_new(&buffer_bio_method);
    ssl->wbio = BIO_new(&buffer_bio_method);
    assert(ssl->rbio && ssl->wbio);
    ssl->rbuf = ssl->rbio->ptr = tlBufferNew();
    ssl->wbuf = ssl->wbio->ptr = tlBufferNew();

    SSLCONST SSL_METHOD *m = SSLv23_client_method();
    assert(m);
    SSL_CTX* ctx = SSL_CTX_new(m);
    assert(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, null);
    SSL_CTX_set_options(ctx, SSL_OP_ALL);

    ssl->session = SSL_new(ctx);
    assert(ssl->session);

    SSL_set_bio(ssl->session, ssl->rbio, ssl->wbio);
    SSL_set_connect_state(ssl->session);

    return ssl;
}
INTERNAL tlHandle _ssl_rbuf(tlArgs* args) {
    tlSSL* ssl = tlSSLAs(tlArgsTarget(args));
    return bio_buffer(ssl->rbio);
}
INTERNAL tlHandle _ssl_wbuf(tlArgs* args) {
    tlSSL* ssl = tlSSLAs(tlArgsTarget(args));
    return bio_buffer(ssl->wbio);
}
INTERNAL tlHandle _ssl_write(tlArgs* args) {
    tlSSL* ssl = tlSSLAs(tlArgsTarget(args));
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("require a buffer");
    trace("WRITE: %p", ssl);

    int len = SSL_write(ssl->session, tlBufferData(buf), tlBufferSize(buf));
    trace("DIDWRITE: %d", len);
    if (len > 0) {
        tlBufferReadSkip(buf, len);
        return tlFalse;
    }
    int err = SSL_get_error(ssl->session, len);
    switch (err) {
        case SSL_ERROR_NONE: trace("NO ERROR!"); break;
        case SSL_ERROR_ZERO_RETURN: trace("EOF"); break;
        case SSL_ERROR_WANT_READ: trace("WANT READ"); return tlTrue;
        case SSL_ERROR_WANT_X509_LOOKUP: trace("SSL LOOKUP ... oeps"); break;
        case SSL_ERROR_SSL:
            trace("SSL SESSION ERROR");
            print("ssl protocol error: %s", ERR_reason_error_string(ERR_get_error()));
            break;
        case SSL_ERROR_WANT_WRITE: trace("WANT WRITE"); break;
        case SSL_ERROR_WANT_CONNECT: trace("WANT CONNECT"); break;
        case SSL_ERROR_WANT_ACCEPT: trace("WANT ACCEPT"); break;
        case SSL_ERROR_SYSCALL:
            print("ssl syscall error: %s", ERR_reason_error_string(err));
            fatal("syscall");
        default: fatal("missing");
    }
    fatal("oeps");
    return tlFalse;
}
INTERNAL tlHandle _ssl_read(tlArgs* args) {
    tlSSL* ssl = tlSSLAs(tlArgsTarget(args));
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("require a buffer");
    trace("READ: %p", ssl);

    tlBufferBeforeWrite(buf, 5*1024);
    int len = SSL_read(ssl->session, writebuf(buf), canwrite(buf));
    trace("DIDREAD: %d", len);
    if (len > 0) {
        didwrite(buf, len);
        return tlINT(len);
    }
    int err;
    switch ((err = SSL_get_error(ssl->session, len))) {
        case SSL_ERROR_NONE: break;
        case SSL_ERROR_ZERO_RETURN: trace("EOF"); break;
        case SSL_ERROR_WANT_WRITE: trace("WANT WRITE"); break;
        case SSL_ERROR_WANT_X509_LOOKUP: trace("SSL LOOKUP ... oeps"); break;
        case SSL_ERROR_SSL:
            trace("SSL SESSION ERROR");
            print("ssl protocol error: %s", ERR_reason_error_string(ERR_get_error()));
            break;
        case SSL_ERROR_WANT_READ: trace("WANT READ"); break;
        case SSL_ERROR_WANT_CONNECT: trace("WANT CONNECT"); break;
        case SSL_ERROR_WANT_ACCEPT: trace("WANT ACCEPT"); break;
        case SSL_ERROR_SYSCALL:
            print("ssl syscall error: %s", ERR_reason_error_string(err));
            fatal("syscall");
        default: fatal("missing");
    }
    return tlZero;
}

INTERNAL tlHandle _ssl_md5(tlArgs* args) {
    tlString* s = tlStringCast(tlArgsGet(args, 0));
    if (!s) TL_THROW("md5 requires a string");

    unsigned char res[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)tlStringData(s), tlStringSize(s), res);
    return tlBinFromCopy((char*)res, sizeof(res));
}
INTERNAL tlHandle _ssl_sha1(tlArgs* args) {
    tlString* s = tlStringCast(tlArgsGet(args, 0));
    if (!s) TL_THROW("sha1 requires a string");

    unsigned char res[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)tlStringData(s), tlStringSize(s), res);
    return tlBinFromCopy((char*)res, sizeof(res));
}

static tlKind _tlSSLKind = {
    .name = "SSL",
    .locked = true,
};
tlKind* tlSSLKind = &_tlSSLKind;

static void openssl_init() {
    // TODO make lazy ...
    SSL_load_error_strings();
    SSL_library_init();

    _tlSSLKind.klass = tlClassMapFrom(
        "rbuf", _ssl_rbuf,
        "wbuf", _ssl_wbuf,
        "read", _ssl_read,
        "write", _ssl_write,
        null
    );
}
static void openssl_init_vm(tlVm* vm) {
    tlMap* SSLStatic = tlClassMapFrom(
        "new", _SSL_new,
        "md5", _ssl_md5,
        "sha1", _ssl_sha1,
        null
    );
    tlVmGlobalSet(vm, tlSYM("SSL"), SSLStatic);
}

