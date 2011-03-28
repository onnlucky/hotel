%{
#include <stdio.h>

#include "tl.h"
#include "debug.h"

#include "platform.h"

typedef struct ParseContext {
    tTask* task;
    tText* text;
    int at;
    int current_indent;
    int indents[100];
} ParseContext;

static inline int writesome(ParseContext* cx, char* buf, int len) {
    int towrite = ttext_size(cx->text) - cx->at;
    if (towrite <= 0) return 0;
    if (towrite < len) len = towrite;
    memcpy(buf, ttext_bytes(cx->text) + cx->at, len);
    cx->at += len;
    return len;
}

#define YYSTYPE tValue
#define YY_XTYPE ParseContext*
#define YY_INPUT(buf, len, max, cx) { len = writesome(cx, buf, max); }

static tValue s_type = null;
static tValue s_send = null;
static tValue s_call = null;

tMap* set_target(tMap* call, tMap* target) {
    if (!call || tmap_is_empty(call)) return target;
    if (tmap_get_sym(call, s_type) == s_send) {
        tMap* pos = tmap_cast(tmap_get_int(call, 2));
        if (!pos) {
            tmap_set_int_(call, 2, target);
            return call;
        } else {
            set_target(pos, target);
            return call;
        }
    }
    if (tmap_get_sym(call, s_type) == s_call) {
        tMap* pos = tmap_cast(tmap_get_int(call, 1));
        if (!pos) {
            tmap_set_int_(call, 1, target);
            return call;
        } else {
            set_target(pos, target);
            return call;
        }
    }
    assert(false);
    return null;
}

#define T yyxvar->task
#define L(l) tlist_as(l)

%}

 start = __ m:map __ !. { $$ = m; }

   map = is:items               { $$ = tmap_from_list(T, L(is)); }
 items = i:item _","__ is:items { $$ = tlist_cat(T, L(i), L(is)); }
       | i:item                 { $$ = i }
       |                        { $$ = tNull; }

  item = "+"_ n:name            { $$ = tLIST2(T, n, tTrue); }
       | "-"_ n:name            { $$ = tLIST2(T, n, tFalse); }
       | n:name _"="__ v:value  { $$ = tLIST2(T, n, v); }
       | v:value                { $$ = tLIST2(T, tNull, v); }

 value = lit | number | text | sym

   lit = "true"      { $$ = tTrue; }
       | "false"     { $$ = tFalse; }
       | "null"      { $$ = tNull; }
       | "undefined" { $$ = tUndefined; }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tINT(atoi(yytext)); }
  text = '"' < (!'"' .)* > '"'      { $$ = ttext_from_copy(T, yytext); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tsym_from_copy(T, yytext); }

slcomment = "//" (!nl .)*
 icomment = "/*" (!"*/" .)* "*/"
  comment = (slcomment nl | icomment)

      eol = _ (nl | ";" | slcomment nl) __
       nl = "\n" | "\r\n" | "\r"
       sp = [ \t]
        _ = (sp | icomment)*
       __ = (sp | nl | comment)*

%%

int find_indent(GREG* G) {
    int indent = 1;
    while (G->pos - indent >= 0) {
        char c = G->buf[G->pos - indent];
        //print("char=%c, pos=%d, indent=%d", c, G->pos, indent);
        if (c == '\n' || c == '\r') break;
        indent++;
    }
    if (G->pos - indent < 0) return 65535;
    indent--;
    return indent;
}
int peek_indent(GREG* G) {
    return G->data->indents[G->data->current_indent];
}
bool push_indent(void* data) {
    GREG* G = (GREG*)data;
    int indent = find_indent(G);
    G->data->current_indent++;
    assert(G->data->current_indent < 100);
    G->data->indents[G->data->current_indent] = indent;
    print("NEW INDENT: %d", peek_indent(G));
    return true;
}
bool pop_indent(void* data) {
    GREG* G = (GREG*)data;
    G->data->current_indent--;
    assert(G->data->current_indent >= 0);
    print("NEW INDENT: %d", peek_indent(G));
    return false;
}
bool check_indent(void* data) {
    GREG* G = (GREG*)data;
    return peek_indent(G) == find_indent(G);
}

tValue parse(tText* text) {
    print("PARSING: %s", t_str(text));

    ParseContext data;
    data.at = 0;
    data.text = text;
    data.current_indent = 0;
    data.indents[0] = 0;

    GREG g;
    yyinit(&g);
    g.data = &data;
    if (!yyparse(&g)) {
        print("ERROR: %d", g.pos + g.offset);
    }
    tValue v = g.ss;
    yydeinit(&g);

    print("PARSED");
    return v;
}

