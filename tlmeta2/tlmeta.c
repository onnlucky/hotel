#include <vm/tl.h>

typedef struct State { int ok; int pos; tlHandle value; } State;
typedef struct Parser {
    int len;
    const char* input;
    int upto;
    const char* anchor;

    uint8_t* out;
    tlHandle value;

    int error_line;
    int error_char;
    int error_pos_begin;
    int error_pos_end;
    int error_line_begin;
    int error_line_end;
    const char* error_rule;
    const char* error_msg;
} Parser;

Parser* parser_new(const char* input, int len) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!len) len = (int)strlen(input);
    parser->len = len;
    parser->input = input;
    parser->out = calloc(1, len + 1);
    return parser;
}

static State r_start(Parser*, int);
bool parser_parse(Parser* p) {
    State s = r_start(p, 0);
    if (!s.ok) {
        return false;
    } else if (p->input[s.pos] != 0) {
        p->error_msg = "incomplete parse";
        return false;
    }
    p->value = s.value;
    return true;
}

typedef State(Rule)(Parser*,int);

static State parser_enter(Parser* p, const char* name, int pos) {
    //print(">> enter: %s %d", name, pos);
    return (State){};
}
static void parser_line_char(Parser* p, int pos, int* line, int* shar) {
    int l = 0; int c = 0;
    for (int i = 0; i < pos; i++) {
        if (p->input[i] == '\n') { l++; c = 0; } else { c++; }
    }
    if (line) *line = l + 1;
    if (shar) *shar = c + 1;
}
static State parser_error(Parser* p, const char* name, int begin, int end) {
    const int pos = end;
    if (p->error_msg) return (State){.pos=pos};
    if (end < begin) end = begin;

    p->error_rule = name;
    p->error_msg = p->anchor;
    parser_line_char(p, end, &p->error_line, &p->error_char);

    p->error_pos_begin = begin;
    p->error_pos_end = end;
    while (begin > 0 && p->input[begin] != '\n') begin--;
    while (end < p->len && p->input[end] != '\n') end++;
    if (begin != 0 && begin < end) begin++;
    p->error_line_begin = begin;
    p->error_line_end = end;

    return (State){.pos=pos};
}
static State parser_fail(Parser* p, const char* name, int pos) {
    //print("<< fail: %s %d", name, pos);
    if (pos < p->upto) p->upto = pos;
    return (State){.pos=pos};
}
static State parser_pass(Parser* p, const char* name, uint8_t number, int start, int pos, tlHandle value) {
    //print("<< pass: %s %d -- %s", name, pos, tl_repr(value));
    //print("<< pass: %s(%d) %d - %d", name, number, start, pos);
    if (number) {
        for (int i = p->upto; i < start; i++) p->out[i] = 0;
        for (int i = start; i < pos; i++) {
            if (!p->out[i] || i >= p->upto) p->out[i] = number;
        }
        p->upto = pos;
    }
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
    int begin = pos;
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
    return parser_pass(p, name, 0, begin, pos, tlArrayToList(res));
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
    int begin = pos;
    parser_enter(p, "star", pos);
    State s = r(p, pos);
    if (!s.ok) return parser_pass(p, "star", 0, begin, pos, tlListEmpty());
    if (!sep && s.pos == pos) return parser_pass(p, "star", 0, begin, pos, tlListFrom1(s.value));
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
    return many("star", res, p, s.pos, r, sep);
}
static State meta_opt(Parser* p, int start, Rule r) {
    parser_enter(p, "opt", start);
    State s = r(p, start);
    if (s.ok) return parser_pass(p, "opt", 0, start, s.pos, s.value);
    return parser_pass(p, "opt", 0, start, start, tlNull);
}
static State meta_not(Parser* p, int start, Rule r) {
    parser_enter(p, "not", start);
    State s = r(p, start);
    if (s.ok) return parser_fail(p, "not", start);
    return parser_pass(p, "not", 0, start, start, tlNull);
}
static State meta_ahead(Parser* p, int start, Rule r) {
    parser_enter(p, "ahead", start);
    State s = r(p, start);
    if (!s.ok) return parser_fail(p, "ahead", start);
    return parser_pass(p, "ahead", 0, start, start, tlNull);
}

