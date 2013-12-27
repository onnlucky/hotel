#include <tl.h>

typedef struct State { const int ok; const int pos; const tlHandle value; } State;
static const State Fail = (State){};
typedef struct Parser { const char* input; } Parser;
typedef State(Rule)(Parser*,int);

static State fail(const char* name, int pos) {
    print("<< fail: %s %d", name, pos);
    return Fail;
}
static State pass(const char* name, int pos, tlHandle value) {
    print("<< pass: %s %d -- %s", name, pos, tl_repr(value));
    return (State){.ok=1,.pos=pos,.value=value};
}

static State prim_char(Parser* p, int pos, const char* chars) {
    int c = p->input[pos];
    if (!c) return Fail;
    while (*chars) {
        if (*chars == c) {
            char buf[2] = {c, 0};
            return (State){1,pos + 1,tlStringFromCopy(buf, 1)};
        }
        chars++;
    }
    return Fail;
}
static State prim_text(Parser* p, int pos, const char* chars) {
    int start = pos;
    while (*chars) {
        int c = p->input[pos];
        if (!c) return Fail;
        if (*chars != c) return Fail;
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
    return pass(name, pos, tlArrayToList(res));
}
static State meta_plus(Parser* p, int pos, Rule r, Rule sep) {
    print(">> plus: %d", pos);
    State s = r(p, pos);
    if (!s.ok) return fail("plus", pos);
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
    return many("plus", res, p, s.pos, r, sep);
}
static State meta_star(Parser* p, int pos, Rule r, Rule sep) {
    print(">> star: %d", pos);
    print("star: %d", pos);
    State s = r(p, pos);
    if (!s.ok) return pass("star", pos, tlListEmpty());
    if (!sep && s.pos == pos) return pass("star", pos, tlListFrom1(s.value));
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
    return many("star", res, p, s.pos, r, sep);
}

static State r_start(Parser*, int);

int main(int argv, char** args) {
    tl_init();

    Parser p = (Parser){ args[1] };
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

