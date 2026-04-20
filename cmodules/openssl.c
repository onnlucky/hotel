#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/ssl.h>

#include "debug.h"
#include <tl.h>

#include "trace-off.h"

#ifdef __APPLE__
#define SSLCONST const
#else
#define SSLCONST const
#endif

static BIO_METHOD *buffer_bio_method = NULL;

static int sb_write(BIO *bio, const char *from, int len) {
    trace("write: %d", len);
    BIO_clear_retry_flags(bio);
    return tlBufferWrite(tlBufferAs(BIO_get_data(bio)), from, len);
}

static int sb_read(BIO *bio, char *into, int len) {
    trace("read: %d", tlBufferSize(tlBufferAs(bio->ptr)));
    BIO_clear_retry_flags(bio);
    int res = tlBufferRead(tlBufferAs(BIO_get_data(bio)), into, len);
    if (res == 0) {
        BIO_set_retry_read(bio);
        return -1;
    }
    return res;
}

static int sb_puts(BIO *bio, const char *str) {
    fatal("oeps");
    return 0;
}

static int sb_gets(BIO *bio, char *into, int len) {
    fatal("oeps");
    return 0;
}

static long sb_ctrl(BIO *bio, int cmd, long num, void *ptr) {
    trace("BUFFER CTRL: %d %ld %p", cmd, num, ptr);
    switch (cmd) {
    case BIO_CTRL_RESET:
        return 0;
    case BIO_CTRL_EOF:
        return 0;
    case BIO_CTRL_INFO:
        return 0;
    case BIO_CTRL_SET:
        fatal("set");
        return 0;
    case BIO_CTRL_GET:
        fatal("get");
        return 0;
    case BIO_CTRL_PUSH:
        return 0;
    case BIO_CTRL_POP:
        return 0;
    case BIO_CTRL_GET_CLOSE:
        return BIO_get_shutdown(bio);
    case BIO_CTRL_SET_CLOSE:
        BIO_set_shutdown(bio, (int)num);
        return 0;
    case BIO_CTRL_PENDING:
        return tlBufferSize(tlBufferAs(BIO_get_data(bio))) > 0;
    case BIO_CTRL_FLUSH:
        return 1;
    case BIO_CTRL_DUP:
        return 1;
    case BIO_CTRL_WPENDING:
        return tlBufferSize(tlBufferAs(BIO_get_data(bio))) > 0;
    case BIO_CTRL_SET_CALLBACK:
        return 0;
    case BIO_CTRL_GET_CALLBACK:
        return 0;
    default:
        fatal("%d", cmd);
        return 0;
    }
    return 0;
}
static int sb_create(BIO *bio) {
    trace("create %p", bio);
    BIO_set_init(bio, 1);
    BIO_set_data(bio, NULL);
    BIO_set_shutdown(bio, 1); // Ensure bio does cleanup when freed
    return 1;
}

static int sb_destroy(BIO *bio) { return 1; }

static long sb_callback_ctrl(BIO *bio, int cmd, bio_info_cb *cb) {
    fatal("oeps");
    return 0;
}

static void setup_buffer_bio_method() {
    if (buffer_bio_method == NULL) {
        buffer_bio_method = BIO_meth_new(BIO_TYPE_FD, "tl-buffer");
        BIO_meth_set_write(buffer_bio_method, sb_write);
        BIO_meth_set_read(buffer_bio_method, sb_read);
        BIO_meth_set_puts(buffer_bio_method, sb_puts);
        BIO_meth_set_gets(buffer_bio_method, sb_gets);
        BIO_meth_set_ctrl(buffer_bio_method, sb_ctrl);
        BIO_meth_set_create(buffer_bio_method, sb_create);
        BIO_meth_set_destroy(buffer_bio_method, sb_destroy);
    }
}

TL_REF_TYPE(tlSSL);
struct tlSSL {
    tlLock lock;
    BIO *rbio;
    BIO *wbio;
    SSL *session;
    tlBuffer *rbuf; // here for GC visibility
    tlBuffer *wbuf; // idem
};

// TODO server state, getting/setting keys and all that good stuff ...
static tlHandle _SSL_new(tlTask *task, tlArgs *args) {
    setup_buffer_bio_method();

    tlSSL *ssl = tlAlloc(tlSSLKind, sizeof(tlSSL));
    ssl->rbio = BIO_new(buffer_bio_method);
    ssl->wbio = BIO_new(buffer_bio_method);
    assert(ssl->rbio && ssl->wbio);
    ssl->rbuf = tlBufferNew();
    BIO_set_data(ssl->rbio, ssl->rbuf);
    ssl->wbuf = tlBufferNew();
    BIO_set_data(ssl->wbio, ssl->wbuf);

    SSLCONST SSL_METHOD *m = SSLv23_client_method();
    assert(m);
    SSL_CTX *ctx = SSL_CTX_new(m);
    assert(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, null);
    SSL_CTX_set_options(ctx, SSL_OP_ALL);

    ssl->session = SSL_new(ctx);
    assert(ssl->session);

    SSL_set_bio(ssl->session, ssl->rbio, ssl->wbio);
    SSL_set_connect_state(ssl->session);

    return ssl;
}

static tlHandle _ssl_rbuf(tlTask *task, tlArgs *args) {
    tlSSL *ssl = tlSSLAs(tlArgsTarget(args));
    return ssl->rbuf;
}

static tlHandle _ssl_wbuf(tlTask *task, tlArgs *args) {
    tlSSL *ssl = tlSSLAs(tlArgsTarget(args));
    return ssl->wbuf;
}

static tlHandle _ssl_write(tlTask *task, tlArgs *args) {
    tlSSL *ssl = tlSSLAs(tlArgsTarget(args));
    tlBuffer *buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf)
        TL_THROW("require a buffer");
    trace("WRITE: %p", ssl);

    int len = SSL_write(ssl->session, tlBufferData(buf), tlBufferSize(buf));
    trace("DIDWRITE: %d", len);
    if (len > 0) {
        tlBufferReadSkip(buf, len);
        return tlFalse;
    }
    int err = SSL_get_error(ssl->session, len);
    switch (err) {
    case SSL_ERROR_NONE:
        trace("NO ERROR!");
        break;
    case SSL_ERROR_ZERO_RETURN:
        trace("EOF");
        break;
    case SSL_ERROR_WANT_READ:
        trace("WANT READ");
        return tlTrue;
    case SSL_ERROR_WANT_X509_LOOKUP:
        trace("SSL LOOKUP ... oeps");
        break;
    case SSL_ERROR_SSL:
        trace("SSL SESSION ERROR");
        print("ssl protocol error: %s", ERR_reason_error_string(ERR_get_error()));
        break;
    case SSL_ERROR_WANT_WRITE:
        trace("WANT WRITE");
        break;
    case SSL_ERROR_WANT_CONNECT:
        trace("WANT CONNECT");
        break;
    case SSL_ERROR_WANT_ACCEPT:
        trace("WANT ACCEPT");
        break;
    case SSL_ERROR_SYSCALL:
        print("ssl syscall error: %s", ERR_reason_error_string(err));
        fatal("syscall");
    default:
        fatal("missing");
    }
    fatal("oeps");
    return tlFalse;
}
static tlHandle _ssl_read(tlTask *task, tlArgs *args) {
    tlSSL *ssl = tlSSLAs(tlArgsTarget(args));
    tlBuffer *buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf)
        TL_THROW("require a buffer");
    trace("READ: %p", ssl);

    char *writebuf = tlBufferWriteData(buf, 5 * 1024);
    size_t canwrite = tlBufferCanWrite(buf);
    int len = SSL_read(ssl->session, writebuf, canwrite);
    trace("DIDREAD: %d", len);
    if (len > 0) {
        tlBufferDidWrite(buf, len);
        return tlINT(len);
    }
    int err;
    switch ((err = SSL_get_error(ssl->session, len))) {
    case SSL_ERROR_NONE:
        break;
    case SSL_ERROR_ZERO_RETURN:
        trace("EOF");
        break;
    case SSL_ERROR_WANT_WRITE:
        trace("WANT WRITE");
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
        trace("SSL LOOKUP ... oeps");
        break;
    case SSL_ERROR_SSL:
        trace("SSL SESSION ERROR");
        print("ssl protocol error: %s", ERR_reason_error_string(ERR_get_error()));
        break;
    case SSL_ERROR_WANT_READ:
        trace("WANT READ");
        break;
    case SSL_ERROR_WANT_CONNECT:
        trace("WANT CONNECT");
        break;
    case SSL_ERROR_WANT_ACCEPT:
        trace("WANT ACCEPT");
        break;
    case SSL_ERROR_SYSCALL:
        print("ssl syscall error: %s", ERR_reason_error_string(err));
        fatal("syscall");
    default:
        fatal("missing");
    }
    return tlZero;
}

static tlHandle _ssl_md5(tlTask *task, tlArgs *args) {
    tlString *s = tlStringCast(tlArgsGet(args, 0));
    if (!s)
        TL_THROW("md5 requires a string");

    unsigned char res[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *)tlStringData(s), tlStringSize(s), res);
    return tlBinFromCopy((char *)res, sizeof(res));
}
static tlHandle _ssl_sha1(tlTask *task, tlArgs *args) {
    tlString *s = tlStringCast(tlArgsGet(args, 0));
    if (!s)
        TL_THROW("sha1 requires a string");

    unsigned char res[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)tlStringData(s), tlStringSize(s), res);
    return tlBinFromCopy((char *)res, sizeof(res));
}

static tlKind _tlSSLKind = {
    .name = "SSLCodec",
    .locked = true,
};
tlKind *tlSSLKind = &_tlSSLKind;

static bool init = false;
static void openssl_init() {
    init = true;
    SSL_load_error_strings();
    SSL_library_init();

    _tlSSLKind.klass = tlClassObjectFrom("rbuf", _ssl_rbuf, "wbuf", _ssl_wbuf, "read", _ssl_read, "write", _ssl_write, null);
}

tlHandle tl_load() {
    if (!init)
        openssl_init();
    return tlClassObjectFrom("newCodec", _SSL_new, "md5", _ssl_md5, "sha1", _ssl_sha1, null);
}
