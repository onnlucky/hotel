#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
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
    const char* name;
    int begin;
    int end;
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
    //print("starting: %s", input);
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

#define CHAR(c) parse_char(p, c)
bool parse_char(Parser* p, char c) {
    if (p->input[p->at] != c) return false;
    p->at++;
    return true;
}

#define STRING(s) parser_string(p, s)
bool parser_string(Parser* p, const char* s) {
    if (!s[0]) return true;
    int at = p->at;
    for (int i = 0; s[i]; i++, at++) {
        if (!p->input[at]) return false;
        if (s[i] != p->input[at]) return false;
    }
    p->at = at;
    return true;
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
RULE(indent)
    int c = PEEK();
    if (!(c == '\n' || c == '\r')) REJECT();
    NEXT();
    PARSE(ws);
END_RULE()

RULE(slcomment)
    if (!STRING("//")) REJECT();
    while (true) {
        int c = PEEK();
        if (!c) break;
        if (c == '\n' || c == '\r') break;
        NEXT();
    }
END_RULE()
RULE(mlcomment)
    if (!STRING("/*")) REJECT();
    while (true) {
        if (PARSE(mlcomment)) continue;
        if (STRING("*/")) break;
        if (!PEEK()) break;
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
    PARSE(sign);
    if (CHAR('.')) {
        int c = PEEK();
        if (c < '0' || c > '9') REJECT();
        PARSE(fraction);
    } else {
        int c = PEEK();
        if (c < '0' || c > '9') REJECT();
        PARSE(whole);
        if (CHAR('.')) PARSE(fraction);
    }
    PARSE(exp);
END_RULE()
RULE(string)
    if (!CHAR('"')) REJECT();
    ANCHOR("expect a closing '\"'");
    while (true) {
        int c = PEEK();
        if (c < 32) REJECT();
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
    //if (c == '$') return true;
    return false;
}
RULE(identifier)
    if (!isIdentChar(PEEK(), true)) REJECT();
    NEXT();
    while (true) {
        if (!isIdentChar(PEEK(), false)) break;
        NEXT();
    }
END_RULE()

static inline bool isOperatorChar(int c) {
    if (c <= 32) return false;
    if (c >= 'a' && c <= 'z') return false;
    if (c >= 'A' && c <= 'Z') return false;
    if (c >= '0' && c <= '9') return false;
    if (c == '"' || c == '\'') return false;
    if (c == '(' || c == ')') return false;
    if (c == '[' || c == ']') return false;
    if (c == '{' || c == '}') return false;
    if (c == '_') return false;
    return true;
}
RULE(operator)
    if (!isOperatorChar(PEEK())) REJECT();
    NEXT();
    while (true) {
        if (!isOperatorChar(PEEK())) break;
        NEXT();
    }
END_RULE()

RULE(brace_open)
    if (!CHAR('(')) REJECT();
END_RULE()
RULE(cbrace_open)
    if (!CHAR('{')) REJECT();
END_RULE()
RULE(sbrace_open)
    if (!CHAR('[')) REJECT();
END_RULE()
RULE(brace_close)
    if (!CHAR(')')) REJECT();
END_RULE()
RULE(cbrace_close)
    if (!CHAR('}')) REJECT();
END_RULE()
RULE(sbrace_close)
    if (!CHAR(']')) REJECT();
END_RULE()

RULE(token)
    OR(string);
    OR(decimal);
    OR(identifier);
    OR(operator);
    OR(brace_open);
    OR(cbrace_open);
    OR(sbrace_open);
    OR(brace_close);
    OR(cbrace_close);
    OR(sbrace_close);
    REJECT();
END_RULE()

RULE(tokens)
    while (true) {
        PARSE(ws);
        if (PARSE(indent)) continue;
        if (PARSE(slcomment)) continue;
        if (PARSE(mlcomment)) continue;
        if (PARSE(token)) continue;
        break;
    }
END_RULE()

RULE(start)
    ANCHOR("end");
    PARSE(tokens);
    AND(end);
END_RULE()

const char* readfile(const char* file) {
    int fd = open(file, O_RDONLY, 0);
    if (fd < 0) abort();

    char* data = malloc(1024*1024); // 1 mb

    int len = 0;
    while (true) {
        int r = read(fd, data + len, 1024*1024);
        if (r < 0) abort();
        if (r == 0) break;
        len += r;
    }
    close(fd);

    return data;
}

bool token_is(Parser* p, int at, const char* name) {
    if (at > p->last_token) return false;
    return strcmp(p->tokens[at].name, name) == 0;
}

int pcall(Parser* p, int at) {
    if (!token_is(p, at, "identifier")) return 0;
    at++;
    if (!token_is(p, at, "brace_open")) return 0;
    at++;
    if (!token_is(p, at, "string")) return 0;
    at++;
    if (!token_is(p, at, "brace_close")) return 0;
    at++;
    print("!!CALL!!");
    return at;
}

int main(int argc, char** argv) {
    const char* input = "xxXXx__123; \"(42_\";(11____[1_._33__33__e100; 1; .0)]; 1.";
    if (argc > 1) input = readfile(argv[1]);

    Parser p;
    parser_parse(&p, input, 0);
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

    pcall(&p, 1);

    return 0;
}

