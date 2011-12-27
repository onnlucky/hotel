#define HTTP_PARSER_ALLOW_REWIND
#include "http-parser/http_parser.c"

int on_message_begin(http_parser* parser) {
    print("ON BEGIN");
    return 0;
}
int on_url(http_parser* parser, const char* at, size_t len) {
    //print("ON URL: %s", strndup(at, len));
    return 0;
}
int on_header_field(http_parser* parser, const char* at, size_t len) {
    //print("ON HEADER FIELD: %s", strndup(at, len));
    return 0;
}
int on_header_value(http_parser* parser, const char* at, size_t len) {
    //print("ON HEADER VALUE: %s", strndup(at, len));
    return 0;
}
int on_headers_complete(http_parser* parser) {
    print("ON HEADERS COMPLETE");
    return 0;
}
int on_body(http_parser* parser, const char* at, size_t len) {
    print("ON BODY");
    return 0;
}
int on_message_complete(http_parser* parser) {
    print("ON END");
    return 0;
}

static http_parser_settings settings = {
 .on_message_begin = on_message_begin,
 .on_url = on_url,
 .on_header_field = on_header_field,
 .on_header_value = on_header_value,
 .on_headers_complete = on_headers_complete,
 .on_body = on_body,
 .on_message_complete = on_message_complete,
};

tlValue _http_parse(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("expected a Buffer");
    http_parser *parser = malloc(sizeof(http_parser));
    http_parser_init(parser, HTTP_REQUEST);

    int more = canread(buf->buf) - 47;
    if (more < 1) more = 1;
    while (canread(buf->buf)) {
        print("reading: %d", more);
        if (more > canread(buf->buf)) break;
        size_t len = http_parser_execute(parser, &settings,
                readbuf(buf->buf), more);
        if (len > 0) {
            didread(buf->buf, len);
            more = 1;
        } else {
            more++;
        }
    }
    return tlNull;
}

static const tlNativeCbs __http_natives[] = {
    { "_http_parse", _http_parse },
    { 0, 0 }
};

void http_init() {
    tl_register_natives(__http_natives);
}

