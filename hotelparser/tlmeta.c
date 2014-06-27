#include <vm/tl.h>

typedef struct State {
    int ok;
    int pos;
#ifndef NO_VALUE
    tlHandle value;
#endif
} State;

typedef struct Parser {
    int len;
    const char* input;
    int upto;
    const char* anchor;
    int indent;

    bool in_peek;
    const char* last_rule;
    const char* last_consume;

    uint8_t* out;
#ifndef NO_VALUE
    tlHandle value;
#endif

    int error_line;
    int error_char;
    int error_pos_begin;
    int error_pos_end;
    int error_line_begin;
    int error_line_end;
    const char* error_rule;
    const char* error_msg;
} Parser;

static Parser* parser_new(const char* input, int len) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!len) len = (int)strlen(input);
    parser->len = len;
    parser->input = input;
    parser->out = calloc(1, len + 1);
    parser->indent = -1;
    parser->in_peek = true;
    return parser;
}

static void parser_free(Parser* parser) {
    free(parser->out);
    free(parser);
}

static State r_start(Parser*, int);
static bool parser_parse(Parser* p) {
    State s = r_start(p, 0);
    if (!s.ok) {
        if (!p->error_msg) p->error_msg = "unknown";
        return false;
    } else if (p->input[s.pos] != 0) {
        p->error_msg = "incomplete parse";
        return false;
    }
#ifndef NO_VALUE
    p->value = s.value;
#endif
    return true;
}

static int parser_indent(Parser* p, int pos) {
    int indent = 0;
    int begin = pos;
    while (begin > 0 && p->input[begin] != '\n') begin--;
    if (begin != 0 && begin < pos) begin++;
    for (; begin < pos; begin++) {
        char c = p->input[begin];
        if (c == '\t') {
            indent += 1 << 16;
        } else {
            indent += 1;
        }
    }
    //print("indent: %d", indent);
    return indent;
}

static State state_fail(int pos) {
    return (State){.pos=pos};
}

static State state_ok(int pos, tlHandle value) {
#ifndef NO_VALUE
    return (State){.ok=1,.pos=pos,.value=value};
#else
    return (State){.ok=1,.pos=pos};
#endif
}

typedef State(Rule)(Parser*,int);

static State parser_enter(Parser* p, const char* name, int pos) {
    if (!p->in_peek) p->last_rule = name;
    //print(">> enter: %s %d '%c'", name, pos, p->input[pos]);
    return (State){};
}
static void parser_line_char(Parser* p, int pos, int* pline, int* pchar) {
    int l = 0; int c = 0;
    for (int i = 0; i < pos; i++) {
        if (p->input[i] == '\n') { l++; c = 0; } else { c++; }
    }
    if (pline) *pline = l + 1;
    if (pchar) *pchar = c + 1;
}

static const char* parser_set_anchor(Parser* p, const char* anchor) {
    const char* prev = p->anchor;
    p->anchor = anchor;
    //print("SET ANCHOR %s", anchor);
    return prev;
}

static State parser_error(Parser* p, const char* name, int begin, int end) {
    const int pos = end;
    if (p->error_msg) return state_fail(pos);
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

    return state_fail(pos);
}

static State parser_fail(Parser* p, const char* name, int pos) {
    if (!p->in_peek) {
        const char* t = p->last_consume;
        if (!t) t = p->last_rule;
        if (t) print("<< fail: %s %d -- expected: %s", name, pos, t);
        p->last_consume = null;
        p->last_rule = null;
    }
    if (pos < p->upto) p->upto = pos;
    return state_fail(pos);
}

static State parser_pass(Parser* p, const char* name, uint8_t number, int start, State state) {
    //print("<< pass: %s %d -- %s", name, state.pos, tl_repr(state.value));
    //print("<< pass: %s(%d) %d - %d", name, number, start, state.pos);
    if (number) {
        for (int i = p->upto; i < start; i++) p->out[i] = 0;
        for (int i = start; i < state.pos; i++) {
            if (!p->out[i] || i >= p->upto) p->out[i] = number;
        }
        p->upto = state.pos;
    }
    return state;
}

static State prim_pos(Parser* p, int pos) {
    return state_ok(pos, tlINT(pos));
}

static State prim_any(Parser* p, int pos) {
    int c = p->input[pos];
    if (!c) return state_fail(pos);
    return state_ok(pos + 1, tlINT(c));
}

static State prim_ws(Parser* p, int pos) {
    while (1) {
        int c = p->input[pos];
        if (c == 0 || c > 32 || c == '\r' || c == '\n') break;
        pos++;
    }
    return state_ok(pos, tlNull);
}

static State prim_wsnl(Parser* p, int pos) {
    while (1) {
        int c = p->input[pos];
        if (c == 0 || c > 32) break;
        pos++;
    }
    return state_ok(pos, tlNull);
}

static State prim_char(Parser* p, int pos, const char* chars) {
    int c = p->input[pos];
    if (!c) return state_fail(pos);
    while (*chars) {
        if (*chars == c) {
            return state_ok(pos + 1, tlINT(c));
        }
        chars++;
    }
    return state_fail(pos);
}

static State prim_text(Parser* p, int pos, const char* chars) {
    if (!p->in_peek) p->last_consume = chars;
    int start = pos;
    while (*chars) {
        int c = p->input[pos];
        if (!c) return state_fail(start);
        if (*chars != c) return state_fail(start);
        pos++; chars++;
    }
    int len = pos - start;
    char* buf = malloc(len + 1);
    memcpy(buf, p->input + start, len);
    buf[len] = 0;

    p->last_consume = null;

#ifndef NO_VALUE
    return state_ok(pos, tlStringFromTake(buf, len));
#else
    return state_ok(pos, tlNull);
#endif
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
#ifndef NO_VALUE
        tlArrayAdd(res, s.value);
#endif
        if (pos == start) break;
    }
#ifndef NO_VALUE
    return parser_pass(p, name, 0, begin, state_ok(pos, tlArrayToList(res)));
#else
    return parser_pass(p, name, 0, begin, state_ok(pos, tlNull));
#endif
}
static State meta_plus(Parser* p, int pos, Rule r, Rule sep) {
    parser_enter(p, "plus", pos);
    State s = r(p, pos);
    if (!s.ok) return parser_fail(p, "plus", pos);
#ifndef NO_VALUE
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
#else
    tlArray* res = null;
#endif
    p->in_peek = true;
    State ret = many("plus", res, p, s.pos, r, sep);
    //p->in_peek = false;
    return ret;
}
static State meta_star(Parser* p, int pos, Rule r, Rule sep) {
    int begin = pos;
    parser_enter(p, "star", pos);
    p->in_peek = true;
    State s = r(p, pos);
    //p->in_peek = false;
#ifndef NO_VALUE
    if (!s.ok) return parser_pass(p, "star", 0, begin, state_ok(pos, tlListEmpty()));
#else
    if (!s.ok) return parser_pass(p, "star", 0, begin, state_ok(pos, tlNull));
#endif
    if (!sep && s.pos == pos) {
#ifndef NO_VALUE
        return parser_pass(p, "star", 0, begin, state_ok(pos, tlListFrom1(s.value)));
#else
        return parser_pass(p, "star", 0, begin, state_ok(pos, tlNull));
#endif
    }
#ifndef NO_VALUE
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
#else
    tlArray* res = null;
#endif
    p->in_peek = true;
    State ret = many("star", res, p, s.pos, r, sep);
    //p->in_peek = false;
    return ret;
}
static State meta_opt(Parser* p, int start, Rule r) {
    parser_enter(p, "opt", start);
    State s = r(p, start);
    if (s.ok) return parser_pass(p, "opt", 0, start, s);
    return parser_pass(p, "opt", 0, start, state_ok(start, tlNull));
}
static State meta_not(Parser* p, int start, Rule r) {
    p->in_peek = true;
    parser_enter(p, "not", start);
    State s = r(p, start);
    //p->in_peek = false;
    if (s.ok) {
        return parser_fail(p, "not", start);
    }
    return parser_pass(p, "not", 0, start, state_ok(start, tlNull));
}
static State meta_ahead(Parser* p, int start, Rule r) {
    parser_enter(p, "ahead", start);
    State s = r(p, start);
    if (!s.ok) return parser_fail(p, "ahead", start);
    return parser_pass(p, "ahead", 0, start, state_ok(start, tlNull));
}

#ifndef NO_VALUE

// ** tools **

/// add an element to the front of a tlList
static tlList* prepend(tlHandle list, tlHandle element) {
    return tlListPrepend(tlListAs(list), element);
}

/// take a list of lists and return a list with all sublist concatenated
static tlList* flatten(tlHandle list) {
    tlList* l = tlListAs(list);
    int size = tlListSize(l);
    if (size == 0) return tlListEmpty();
    tlList* res = tlListGet(l, 0);
    for (int i = 1; i < size; i++) {
        res = tlListCat(res, tlListGet(l, i));
    }
    return res;
}

/// make a string from a list of characters (tlList of tlNumber's)
static tlString* String(tlHandle list) {
    tlList* l = tlListAs(list);
    int n = tlListSize(l);
    char buf[n + 1];
    for (int i = 0; i < n; i++) {
        buf[i] = tl_int(tlListGet(l, i));
    }
    buf[n] = 0;
    return tlStringFromCopy(buf, n);
}

static int digitFromChar(int c, int radix) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    return 0;
}

/// make a float from: a tlINT(1/-1), tlList(tlINT('0'-'9')), tlList(tlINT('0'-'9')), radix
static tlFloat* Float(tlHandle s, tlHandle whole, tlHandle frac, int radix) {
    double res = 0;
    if (whole) {
        tlList* l = tlListAs(whole);
        for (int i = 0; i < tlListSize(l); i++) {
            res = res * radix + digitFromChar((int)tl_int(tlListGet(l, i)), radix);
        }
    }
    if (frac) {
        double n = 0;
        double d = 1;
        tlList* l = tlListAs(frac);
        for (int i = 0; i < tlListSize(l); i++) {
            n = n * radix + digitFromChar((int)tl_int(tlListGet(l, i)), radix);
            d *= radix;
        }
        res += n / d;
    }
    res *= tl_int(s);
    return tlFLOAT(res);
}

/// make a number from: a tlINT(1/-1), tlList(tlINT('0'-'9')), radix
static tlHandle Number(tlHandle s, tlHandle whole, int radix) {
    tlList* l = tlListAs(whole);
    int n = tlListSize(l);
    char buf[n + 2];
    buf[0] = (s && tl_int(s) < 0)? '-' : '+';
    for (int i = 0; i < n; i++) {
        buf[i + 1] = tl_int(tlListGet(l, i));
    }
    buf[n + 1] = 0;
    return tlPARSENUM(buf, radix);
}


// ** transforms **

static tlHandle process_tail(tlHandle value, tlHandle tail) {
    if (tail == tlNull) return value;
    tlHandle target = tlObjectGet(tail, tlSYM("target"));
    return tlObjectSet(tail, tlSYM("target"), process_tail(value, target));
}

static tlHandle process_assert(tlHandle args, tlHandle tail, tlHandle pos, tlHandle begin, tlHandle end, Parser* parser) {
    int b = tl_int(begin);
    int e = tl_int(end);
    tlString* data = tlStringFromCopy(parser->input + b, e - b);
    tlHandle text = tlObjectFrom("n", tlSTR("string"), "v", tlObjectFrom("type", tlSTR("string"), "data", data, null), null);
    args = tlListAppend(tlListAs(args), text);
    tlHandle target = tlObjectFrom("type", tlSTR("ref"), "name", tlSTR("assert"), "pos", pos, null);
    tlHandle value = tlObjectFrom("target", target, "args", args, "type", tlSTR("call"), "pos", begin, "endpos", end, null);
    return process_tail(value, tail);
}

static tlHandle process_call(tlHandle args, tlHandle tail, tlHandle pos) {
    tlHandle value = tlObjectFrom("target", tlNull, "args", args,
            "type", tlSTR("call"), "pos", pos, null);
    return process_tail(value, tail);
}

static tlHandle process_method(tlHandle type, tlHandle method, tlHandle args, tlHandle tail, tlHandle pos) {
    tlHandle ttype = tlNull;
    if (strcmp("::", tlStringData(type)) == 0) ttype = tlSYM("classmethod");
    else if (strcmp("?", tlStringData(type)) == 0) ttype = tlSYM("savemethod");
    else if (strcmp("!", tlStringData(type)) == 0) ttype = tlSYM("asyncmethod");
    else ttype = tlSYM("method");

    tlHandle value = tlObjectFrom("target", tlNull, "args", args, "method", method,
            "type", ttype, "pos", pos, null);
    return process_tail(value, tail);
}

static tlHandle process_set_field(tlHandle field, tlHandle value, tlHandle pos) {
    tlHandle f = tlObjectFrom("v", tlObjectFrom("type", tlSTR("string"), "data", field, null), null);
    return tlObjectFrom("target", tlNull, "args", tlListFrom2(f, value), "method", tlSYM("_set"),
            "type", tlSYM("method"), "pos", pos, null);
}

static tlHandle process_get(tlHandle key, tlHandle tail, tlHandle pos) {
    tlHandle value = tlObjectFrom("target", tlNull, "args", tlListFrom1(key), "method", tlSYM("get"),
            "type", tlSYM("method"), "pos", pos, null);
    return process_tail(value, tail);
}

static tlHandle process_set(tlHandle key, tlHandle value, tlHandle pos) {
    return tlObjectFrom("target", tlNull, "args", tlListFrom2(key, value), "method", tlSYM("set"),
            "type", tlSYM("method"), "pos", pos, null);
}

static tlHandle process_slice(tlHandle from, tlHandle to, tlHandle tail, tlHandle pos) {
    tlHandle value = tlObjectFrom("target", tlNull, "args", tlListFrom2(from, to), "method", tlSYM("slice"),
            "type", tlSYM("method"), "pos", pos, null);
    return process_tail(value, tail);
}

static tlHandle process_expr(tlHandle lhs, tlHandle rhs) {
    if (rhs == tlNull) return lhs;
    return tlObjectFrom("op", tlObjectGet(rhs, tlSYM("op")), "lhs", lhs, "rhs", tlObjectGet(rhs, tlSYM("r")),
            "type", tlSYM("op"), "pos", tlObjectGet(rhs, tlSYM("pos")), null);
}

static tlHandle process_mcall(tlHandle ref, tlHandle arg, tlHandle pos) {
    return tlObjectFrom("target", ref, "args", tlListFrom1(arg),
            "type", tlSTR("call"), "pos", pos, null);
}

static tlHandle process_add_block(tlHandle call, tlHandle block, tlHandle pos) {
    if (block == tlNull) return call;
    if (!tlObjectIs(call) || !tlObjectGet(call, tlSYM("target"))) {
        call = tlObjectFrom("target", call, "args", tlListEmpty(),
                "type", tlSTR("call"), "pos", pos, null);
    }
    return tlObjectSet(call, tlSYM("block"), block);
}

static tlHandle _parser_parse(tlArgs* args) {
    tlString* code = tlStringCast(tlArgsGet(args, 0));
    if (!code) TL_THROW("expected a String");
    Parser* p = parser_new(tlStringData(code), tlStringSize(code));
    bool r = parser_parse(p);
    if (!r) {
        TL_THROW("parse error: %s at: %d:%d", p->error_msg, p->error_line, p->error_char);
    }
    return p->value;
}

static void parser_init() {
    tl_register_global("parse", tlNATIVE(_parser_parse, "parse"));
}

#endif
