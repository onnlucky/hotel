%{
#include <stdio.h>

typedef struct ParseContext {
    tTask* task;
    tText* text;
    int at;
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
#define YY_INPUT(buf, len, max) { len = writesome(yyxvar, buf, max); }

#define L(l) tlist_as(l)
#define TASK yyxvar->task

%}

  body = ts:stms           { $$ = tlist_prepend(TASK, L(ts), _BODY_); }
  stms = t:stm eol ts:body { $$ = tlist_prepend(TASK, L(ts), t); }
       | t:stm             { $$ = tlist_new_add(TASK, t); }
       |                   { $$ = tlist_new(TASK, 0); }

   stm = expr
  expr = paren
 paren = "("__ body __")"
       | call
       | value

  call = n:ref _ "("__ as:cargs __")"  { $$ = tlist_prepend2(TASK, L(as), _CALL_, n); }
 cargs = e:expr __","__ as:cargs       { $$ = tlist_prepend(TASK, L(as), e); }
       | e:expr                        { $$ = tlist_new_add(TASK, e) }
       |                               { $$ = tlist_new(TASK, 0); }

 value = lit | number | text | ref

   ref = n:name      { $$ = tlist_new_add2(TASK, _REF_, n); }
   lit = "true"      { $$ = tTrue; }
       | "false"     { $$ = tFalse; }
       | "null"      { $$ = tNull; }
       | "undefined" { $$ = tUndef; }

number = < [0-9]+ >                 { $$ = tINT(atoi(yytext)); }
  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tSYM(yytext); }
  text = '"' < (!'"' .)* > '"'      { $$ = tTEXT(yytext); }

slcomment = "//" (!nl .)*
 icomment = "/*" (!"*/" .)* "*/"
  comment = (slcomment nl | icomment)

      eol = _ (nl | ";" | slcomment nl) __
       nl = "\n" | "\r\n" | "\r"
       sp = [ \t]
        _ = (sp | icomment)*
       __ = (sp | nl | comment)*

%%

void yyinit(GREG *G) {
    memset(G, 0, sizeof(GREG));
}

void yydeinit(GREG *G) {
    free(G->buf);
    free(G->text);
    free(G->thunks);
    free(G->vals);
}

int main() {
    ParseContext cx;
    cx.task = task_new();
    cx.at = 0;
    cx.text = tTEXT("print(42,43,44,foo(999))");

    GREG g;
    yyinit(&g);
    g.data = &cx;
    while(yyparse(&g));
    Value v = g.ss;
    yydeinit(&g);

    tFun* fun = newBody(task, tList_as(v));

    return 0;
}

