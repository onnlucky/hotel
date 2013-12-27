#include <tl.h>

typedef struct State { const int ok; const int pos; const tlHandle value; } State;
static const State Fail = (State){};
typedef struct Parser { const char* input; } Parser;
typedef State(Rule)(Parser*,State);

static State fail(const char* name, State s) {
    print("<< fail: %s", name);
    return s;
}
static State pass(const char* name, State s) {
    print("<< pass: %s %d", name, s.pos);
    return s;
}

static State prim_char(Parser* p, State s, const char* chars) {
    int c = p->input[s.pos];
    if (!c) return Fail;
    while (*chars) {
        if (*chars == c) {
            char buf[2] = {c, 0};
            return (State){1,s.pos + 1,tlStringFromCopy(buf, 1)};
        }
        chars++;
    }
    return Fail;
}
static State prim_text(Parser* p, State s, const char* chars) {
    int pos = s.pos;
    while (*chars) {
        int c = p->input[pos];
        if (!c) return Fail;
        if (*chars != c) return Fail;
        pos++; chars++;
    }
    int len = pos - s.pos;
    char* buf = malloc(len + 1);
    memcpy(buf, p->input + s.pos, len);
    buf[len] = 0;

    return (State){1,pos,tlStringFromTake(buf, len)};
}
static State meta_plus(Parser* p, State s, Rule r) {
    print(">> plus: %d", s.pos);
    s = r(p, s);
    if (!s.ok) return fail("plus", Fail);
    tlArray* res = tlArrayNew();
    tlArrayAdd(res, s.value);
    while (1) {
        State next = r(p, s);
        if (!next.ok) return pass("plus", (State){1,s.pos,tlArrayToList(res)});
        tlArrayAdd(res, next.value);
        s = next;
    }
    return Fail;
}
static State meta_star(Parser* p, State s, Rule r) {
    print(">> star: %d", s.pos);
    tlArray* res = tlArrayNew();
    while (1) {
        State next = r(p, s);
        if (!next.ok) return pass("star", (State){1,s.pos,tlArrayToList(res)});
        tlArrayAdd(res, next.value);
        s = next;
    }
    return Fail;
}
static State meta_starsep(Parser* p, State s, Rule r, Rule sep) {
    print(">> starsep: %d", s.pos);
    return fail("starsep", Fail);
}

static State r_start(Parser* p, State s);

int main(int argv, char** args) {
    tl_init();

    Parser p = (Parser){ args[1] };
    print("starting");
    State s = r_start(&p, (State){1,0});
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

