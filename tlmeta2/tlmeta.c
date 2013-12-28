#include <tl.h>

typedef struct State { int ok; int pos; tlHandle value; } State;
typedef struct Parser {
    const char* input;
    const char* anchor;
    int error_line;
    int error_char;
    const char* error_rule;
    const char* error_msg;
} Parser;
typedef State(Rule)(Parser*,int);

static State parser_enter(Parser* p, const char* name, int pos) {
    print(">> enter: %s %d", name, pos);
    return (State){};
}
static State parser_error(Parser* p, const char* name, int pos) {
    print("<< error: %s %d", name, pos);
    // figure out line and char
    int l = 1;
    int c = 0;
    for (int i = 0; i < pos; i++) {
        if (p->input[i] == '\n') { l++; c = 0; } else { c++; }
    }
    p->error_line = l;
    p->error_char = c;
    p->error_rule = name;
    p->error_msg = p->anchor;
    print("!! ERROR !! %s line: %d char: %d", p->anchor, l, c);
    return (State){.pos=pos};
}
static State parser_fail(Parser* p, const char* name, int pos) {
    print("<< fail: %s %d", name, pos);
    return (State){.pos=pos};
}
static State parser_pass(Parser* p, const char* name, int pos, tlHandle value) {
    print("<< pass: %s %d -- %s", name, pos, tl_repr(value));
    return (State){.ok=1,.pos=pos,.value=value};
}

static State prim_any(Parser* p, int pos) {
    int c = p->input[pos];
    if (!c) return (State){.pos=pos};
    return (State){.ok=1,.pos=pos+1,.value=tlINT(c)};
}
static State prim_ws(Parser* p, int pos) {
    while (1) {
        int c = p->input[pos];
        if (c == 0 || c > 32 || c == '\r' || c == '\n') break;
        pos++;
    }
    return (State){.ok=1,.pos=pos,.value=tlNull};
}
static State prim_wsnl(Parser* p, int pos) {
    while (1) {
        int c = p->input[pos];
        if (c == 0 || c > 32) break;
        pos++;
    }
    return (State){.ok=1,.pos=pos,.value=tlNull};
}
static State prim_char(Parser* p, int pos, const char* chars) {
    int c = p->input[pos];
    if (!c) return (State){.pos=pos};
    while (*chars) {
        if (*chars == c) {
            return (State){1,pos + 1,tlINT(c)};
        }
        chars++;
    }
    return (State){.pos=pos};
}
static State prim_text(Parser* p, int pos, const char* chars) {
    int start = pos;
    while (*chars) {
        int c = p->input[pos];
        if (!c) return (State){.pos=start};
        if (*chars != c) return (State){.pos=start};
        pos++; chars++;
    }
    int len = pos - start;
    char* buf = malloc(len + 1);
    memcpy(buf, p->input + start, len);
    buf[len] = 0;

    return (State){1,pos,tlStringFromTake(buf, len)};
}

static State many(const char* name, tlArray* res, Parser* p, int pos, Rule r, Rule sep) {
    while (1) {
        int start = pos;
        if (sep) {
            State s = sep(p, pos);
            if (!s.ok) break;
            pos = s.pos;
        }
        State s = r(p, pos);
        if (!s.ok) {
            pos = start;
            break;
        }
        pos = s.pos;
        tlArrayAdd(res, s.value);
        if (pos == start) break;
    }
    return parser_pass(p, name, pos, tlArrayToList(res));
}
static State meta_plus(Parser* p, int pos, Rule r, Rule sep) {
    parser_enter(p, "plus", pos);
    State s = r(p, pos);
    if (!s.ok) return parser_fail(p, "plus", pos);
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
    return many("plus", res, p, s.pos, r, sep);
}
static State meta_star(Parser* p, int pos, Rule r, Rule sep) {
    parser_enter(p, "star", pos);
    State s = r(p, pos);
    if (!s.ok) return parser_pass(p, "star", pos, tlListEmpty());
    if (!sep && s.pos == pos) return parser_pass(p, "star", pos, tlListFrom1(s.value));
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
    return many("star", res, p, s.pos, r, sep);
}
static State meta_opt(Parser* p, int pos, Rule r) {
    parser_enter(p, "opt", pos);
    State s = r(p, pos);
    if (s.ok) return parser_pass(p, "opt", s.pos, s.value);
    return parser_pass(p, "opt", pos, tlNull);
}
static State meta_not(Parser* p, int pos, Rule r) {
    parser_enter(p, "not", pos);
    State s = r(p, pos);
    if (s.ok) return parser_fail(p, "not", pos);
    return parser_pass(p, "not", pos, tlNull);
}
static State meta_ahead(Parser* p, int pos, Rule r) {
    parser_enter(p, "ahead", pos);
    State s = r(p, pos);
    if (!s.ok) return parser_fail(p, "ahead", pos);
    return parser_pass(p, "ahead", pos, tlNull);
}

static State r_start(Parser*, int);

int main(int argv, char** args) {
    tl_init();

    tlBuffer* buf = tlBufferFromFile(args[1]);
    Parser p = (Parser){ tlBufferData(buf) };
    print("starting");
    State s = r_start(&p, 0);
    if (!s.ok) {
        print("error: at: %d", s.pos);
        return 1;
    } else if (p.input[s.pos] != 0) {
        print("error: incomplete parse at: %d", s.pos);
        return 1;
    }

    print("%s", tl_repr(s.value));
    return 0;
}

