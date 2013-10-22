#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TRACE

#define print(fmt, x...) ((void)(fprintf(stdout, fmt"\n", ##x), fflush(stdout), 0))
#ifdef TRACE
#define trace(fmt, x...) ((void)(fprintf(stderr, fmt"\n", ##x), fflush(stderr), 0))
#else
#define trace(fmt, x...)
#endif

#define null 0
#define true 1
#define false 0
typedef int bool;

typedef struct Token {
    int begin;
    int end;
    const char* name;
    bool anchor;
} Token;

typedef struct Parser {
    const char* input;
    int len;
    int at;

    int error;
    int error_char;
    const char* error_rule;
    const char* error_reason;

    Token* tokens;
    int tokens_len;
    int last_token;
} Parser;

#define STR2(x) #x
#define STR(x) STR2(x)
#define HAVE_RULE(name) int rule_##name(Parser*)
#define RULE(name) int rule_##name(Parser* p){\
    static const char __name[]=#name;\
    if (p->error) return false;\
    int __token=parser_rule_enter(p, __name);
#define END_RULE() __end: return parser_rule_accept(p, __token, __name); goto __end;}
#define REJECT() return parser_rule_reject(p, __token, __name)
#define ANCHOR(reason) parser_anchor(p, __token, reason)

#define PARSE(name) rule_##name(p)
#define AND(name) if (!rule_##name(p)) REJECT()
#define OR(name) if (rule_##name(p)) goto __end;

#define RECURSE(name) parser_check_recurse(p, name)
#define PEEK_AHEAD(n) parser_peek_ahead(p, n)
#define PEEK() parser_peek(p)
#define NEXT() parser_next(p)
#define EXPECT(c) parser_expect(p, c)

HAVE_RULE(start);
int parser_parse(Parser* p, const char* input, int len) {
    print("starting: %s", input);
    p->input = input;
    p->len = (len)? len : strlen(input);
    p->at = 0;

    p->error = 0;
    p->error_char = 0;

    p->tokens_len = 0;
    p->tokens = null;
    p->last_token = -1;
    return rule_start(p);
}
void parser_anchor(Parser* p, int token, const char* reason) {
    p->tokens[token].anchor = true;
    p->tokens[token].name = reason;
}
int parser_rule_enter(Parser* p, const char* name) {
    p->last_token += 1;
    if (p->last_token == p->tokens_len) {
        p->tokens_len *= 2;
        if (!p->tokens_len) p->tokens_len = 256;
        p->tokens = realloc(p->tokens, p->tokens_len * sizeof(Token));
    }
    int token = p->last_token;
    p->tokens[token].begin = p->at;
    p->tokens[token].name = name;
    print(" try: %s", name);
    return token;
}
int parser_rule_reject(Parser* p, int token, const char* name) {
    trace("fail: %s", name);
    if (!p->error) {
        if (p->tokens[token].anchor) {
            int l = 1; int c = 0;
            for (int i = 0; i < p->at; i++) {
                if (p->input[i] == '\n') { l++; c = 0; } else { c++; }
            }
            p->error = l;
            p->error_char = c;
            p->error_rule = name;
            p->error_reason = p->tokens[token].name;
            //p->last_token = token;
        } else {
            p->at = p->tokens[token].begin;
            p->last_token = token - 1;
        }
    }
    p->tokens[token].end = 0;
    p->tokens[token].name = name;
    return 0;
}
int parser_rule_accept(Parser* p, int token, const char* name) {
    trace("  OK: %s (%d-%d)", name, p->tokens[token].begin, p->at);
    // skip zero char consuming rules, like ws
    if (token == p->last_token) {
        if (p->tokens[token].begin == p->at || !strcmp(name, "wsnl") || !strcmp(name, "ws")) {
            p->last_token = token - 1;
            return 1;
        }
    }
    p->tokens[token].name = name;
    p->tokens[token].end = p->at;
    return 1;
}

int parser_peek_ahead(Parser* p, int ahead) {
    if (p->at + ahead >= p->len) return 0;
    return p->input[p->at + ahead];
}
int parser_peek(Parser* p) {
    if (p->at >= p->len) return 0;
    return p->input[p->at];
}
bool parser_expect(Parser* p, int c) {
    print("EXPECT: %c", c);
    return c == parser_peek(p);
}
void parser_next(Parser* p) {
    p->at += 1;
}
int parser_get_indent(Parser* p) {
    int i = 0;
    while (i <= p->at && p->input[p->at - i] != '\n') i++;
    return i;
}
bool parser_check_recurse(Parser* p, const char* rule) {
    int begin = p->tokens[p->last_token].begin;
    for (int i = p->last_token - 1; i >= 0; i--) {
        if (p->tokens[i].begin != begin) return false;
        print("%s", p->tokens[i].name);
        if (!strcmp(p->tokens[i].name, "value")) return true;
    }
    return false;
}

RULE(end)
    if (PEEK()) REJECT();
END_RULE()
RULE(wsnl)
    while (true) {
        int c = PEEK();
        if (!c || c > 32) break;
        NEXT();
    }
END_RULE()
RULE(ws)
    while (true) {
        int c = PEEK();
        if (!c || c > 32) break;
        if (c == '\n' || c == '\r') break;
        NEXT();
    }
END_RULE()

RULE(sign)
    int c = PEEK();
    if (c == '+' || c == '-') NEXT();
END_RULE()
RULE(whole)
    while (true) {
        int c = PEEK();
        if (!(c == '_' || (c >= '0' && c <= '9'))) break;
        NEXT();
    }
END_RULE()
RULE(fraction)
    while (true) {
        int c = PEEK();
        if (!(c == '_' || (c >= '0' && c <= '9'))) break;
        NEXT();
    }
END_RULE()
RULE(exp)
    int c = PEEK();
    if (c == 'e' || c == 'E') {
        ANCHOR("expect number");
        NEXT();
        c = PEEK();
        if (c == '+' || c == '-') {
            NEXT();
            c = PEEK();
        }
        if (c < '0' || c > '9') REJECT();
        NEXT();
        while (true) {
            c = PEEK();
            if (!(c == '_' || (c >= '0' && c <= '9'))) break;
            NEXT();
        }
    }
END_RULE()
RULE(decimal)
    int c;
    PARSE(sign);
    c = PEEK();
    if (c == '.') {
        PARSE(whole);
        NEXT();
        c = PEEK();
        if (c < '0' || c > '9') REJECT();
        PARSE(fraction);
    } else {
        c = PEEK();
        if (c < '0' || c > '9') REJECT();
        PARSE(whole);
        c = PEEK();
        if (c == '.') {
            NEXT();
            PARSE(fraction);
        }
    }
    PARSE(exp);
END_RULE()
RULE(string)
    int c = PEEK();
    if (c != '"') REJECT();
    NEXT();
    ANCHOR("expect a closing '\"'");
    while (true) {
        c = PEEK();
        if (!c) REJECT();
        NEXT();
        if (c == '"') break;
    }
END_RULE()

static inline bool isIdentChar(int c, bool start) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c == '_') return true;
    if (start) return false;
    if (c >= '0' && c <= '9') return true;
    return false;
}
RULE(identifier)
    int c = PEEK();
    if (!isIdentChar(c, true)) REJECT();
    NEXT();
    while (true) {
        c = PEEK();
        if (!isIdentChar(c, false)) break;
        NEXT();
    }
END_RULE()

HAVE_RULE(value);
HAVE_RULE(block);

RULE(args)
    bool some = false;
    if (EXPECT('(')) {
        some = true;
        NEXT();
        ANCHOR("expect ')'");
        while (true) {
            PARSE(ws);
            if (!PARSE(value)) break;
            PARSE(ws);
            if (!EXPECT(',')) break;
            NEXT();
        }
        if (!EXPECT(')')) REJECT();
        NEXT();
    }
    if (EXPECT(':')) {
        some = true;
        NEXT();
        ANCHOR("expect a block");
        PARSE(ws);
        PARSE(block);
    }
    if (!some) REJECT();
END_RULE()

RULE(prim)
    OR(string);
    OR(decimal);
    OR(identifier);
    REJECT();
END_RULE()

RULE(method)
    if (RECURSE("value")) {
        AND(prim);
    } else {
        AND(value);
    }
    PARSE(ws);
    if (!EXPECT('.')) REJECT();
    NEXT();
    ANCHOR("method name");
    PARSE(ws);
    AND(identifier);
    PARSE(ws);
    // TODO optional
    OR(args);
END_RULE()

RULE(call)
    if (RECURSE("value")) {
        AND(prim);
    } else {
        AND(value);
    }
    PARSE(ws);
    AND(args);
END_RULE()

RULE(value)
    OR(call);
    OR(method);
    OR(prim);
    REJECT();
END_RULE()

RULE(line)
    while (true) {
        AND(value);
        PARSE(ws);
        if (PEEK() != ';') break;
        NEXT();
        ANCHOR("a statement");
        PARSE(ws);
    }
END_RULE()

RULE(body)
    PARSE(wsnl);
    int indent = parser_get_indent(p);
    AND(line);
    ANCHOR("';' or newline");
    while (true) {
        PARSE(wsnl);
        if (PARSE(end)) break;
        if (parser_get_indent(p) < indent) break;
        if (parser_get_indent(p) > indent) REJECT();
        AND(line);
    }
END_RULE()

RULE(block)
    AND(body);
END_RULE()

RULE(start)
    ANCHOR("end");
    AND(body);
    PARSE(ws);
    AND(end);
END_RULE()

int main(int argc, char** argv) {
    Parser p;
    parser_parse(&p, "xxXXx__123; \"42_\";11____1_._33__33__e100; 1; .0; 1.", 0);
    if (p.error) {
        print("error: line %d, char: %d", p.error, p.error_char);

        int start = p.at - p.error_char;
        char* ends = strstr(p.input + start, "\n");
        int end = ends - (p.input + start);
        if (!ends) end = strlen(p.input + start);
        char* line = strndup(p.input + start, end);
        print("  %s", line);
        char* errline = malloc(p.error_char + 1);
        memset(errline, 32, p.error_char);
        errline[p.error_char] = '^';
        errline[p.error_char + 1] = 0;
        print("  %s", errline);
        free(line);
        free(errline);

        if (p.error_reason) {
            print("%s (%s)", p.error_reason, p.error_rule);
        }

        for (int i = p.last_token; i >= 0; i--) {
            print("error: %s (%d-%d)", p.tokens[i].name, p.tokens[i].begin, p.tokens[i].end);
        }
        return 0;
    }

    print("parsed ok; collapsing:");
    // TODO somewhow this should/must work better, perhaps also have list of unwanted rules?
    // collapse
    int j = 0;
    for (int i = 0; i <= p.last_token; i++) {
        if (p.tokens[j].begin == p.tokens[i].begin && p.tokens[j].end == p.tokens[i].end) {
            p.tokens[j] = p.tokens[i];
        } else {
            j++;
            p.tokens[j] = p.tokens[i];
        }
    }
    p.last_token = j;

    print("tokens:");
    char buf[80];
    for (int i = 0; i <= p.last_token; i++) {
        int len = p.tokens[i].end - p.tokens[i].begin;
        if (len > sizeof(buf)) len = sizeof(buf);
        memcpy(buf, p.input + p.tokens[i].begin, len);
        buf[len] = 0;
        for (int i = 0; i < len; i++) if (buf[i] < 32) buf[i] = '$';
        print(" %10s (%3d-%3d) -- %s", p.tokens[i].name, p.tokens[i].begin, p.tokens[i].end, buf);
    }
    print("%zd", p.last_token * sizeof(Token));


    // value[prim, args] -> call(prim, args)
    // value X tail X
    for (int i = 0; i <= p.last_token; i++) {
        if (!strcmp(p.tokens[i].name, "value")) {
            print("call");
            //find_caller(p, i);
            //find_
           // emit_value(p, i, p.tokens[i].end);
        }
    }

    return 0;
}

