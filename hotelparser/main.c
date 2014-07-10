#include "hotelparser.c"

int main(int argv, char** args) {
    tl_init();

    tlBuffer* buf = tlBufferFromFile(args[1]);
    Parser* p = parser_new(tlBufferData(buf), tlBufferSize(buf));
    bool r = parser_parse(p, r_start);
    if (!r) {
        print("error: %s at: %d:%d", p->error_msg, p->error_line, p->error_char);
        return 1;
    }

#ifndef NO_VALUE
    print("%s", tl_repr(p->value));
#endif
    return 0;
}

