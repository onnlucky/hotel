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

tList* set_target(tList* call, tList* target) {
    assert(tlist_size(call) >= 2);
    assert(tlist_get(call, 0) == _CALL_);
    assert(tlist_get(call, 1) == null);
    tlist_set_(call, 1, target);
    return call;
}

%}

 start = __ b:body __ !.   { $$ = b; }

  body = ts:stms           { $$ = tlist_prepend(TASK, L(ts), _BODY_); }
  stms = t:stm eol ts:body { $$ = tlist_prepend(TASK, L(ts), t); }
       | t:stm             { $$ = tlist_new_add(TASK, t); }
       |                   { $$ = tlist_new(TASK, 0); }

   stm = expr
  expr = paren
 paren = "("__ body __")"
       | call
       | value

  tail = "("__ as:cargs __")" t:tail {
           $$ = set_target(L(t), tlist_prepend2(TASK, L(as), _CALL_, null));
       }
       | "("__ as:cargs __")" {
           $$ = tlist_prepend2(TASK, L(as), _CALL_, null);
       }
       | "." n:name "("__ as:cargs __")" t:tail {
           $$ = set_target(L(t), tlist_prepend4(TASK, L(as), _CALL_, _SEND_, null, n));
       }
       | "." n:name "("__ as:cargs __")" {
           $$ = tlist_prepend4(TASK, L(as), _CALL_, _SEND_, null, n)
       }
       | "." n:name t:tail {
           $$ = set_target(L(t), tlist_new_add4(TASK, _CALL_, _SEND_, null, n));
       }
       | "." n:name {
           $$ = tlist_new_add4(TASK, _CALL_, _SEND_, null, n);
       }

  call = n:ref _ "("__ as:cargs __")" t:tail {
           $$ = set_target(L(t), tlist_prepend2(TASK, L(as), _CALL_, n));
       }
       | n:ref _ "("__ as:cargs __")"  { $$ = tlist_prepend2(TASK, L(as), _CALL_, n); }
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
  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tSYM(strdup(yytext)); }
  text = '"' < (!'"' .)* > '"'      { $$ = tTEXT(strdup(yytext)); }

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

tValue compile(tText* text) {
    print("COMPILE: '%s'", t_str(text));
    tTask* task = ttask_new_global();
    ParseContext cx;
    cx.task = task;
    cx.at = 0;
    cx.text = text;

    GREG g;
    yyinit(&g);
    g.data = &cx;
    if (!yyparse(&g)) {
        print("ERROR: %s", g.text + g.pos);
    }
    tValue v = g.ss;
    yydeinit(&g);

    v = newBody(task, tlist_as(v));
    //ttask_free(task);
    return v;
}
