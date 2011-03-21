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

tList* set_target(tList* call, tList* target) {
    if (call == t_list_empty) return target;
    if (tlist_get(call, 1) == _SEND_) {
        tValue v = tlist_get(call, 2);
        if (v == null) {
            tlist_set_(call, 2, target);
            return call;
        } else {
            set_target(tlist_as(v), target);
            return call;
        }
    }
    if (tlist_get(call, 0) == _CALL_) {
        tValue v = tlist_get(call, 1);
        if (v == null) {
            tlist_set_(call, 1, target);
            return call;
        } else {
            set_target(tlist_as(v), target);
            return call;
        }
    }
    assert(false);
}

#define TASK yyxvar->task
#define L(l) tlist_as(l)

#define _CALL1(fn, a1)     tlist_new_add3(TASK, _CALL_, fn, a1)
#define _CALL2(fn, a1, a2) tlist_new_add4(TASK, _CALL_, fn, a1, a2)

#define _CAT(l, r) tlist_cat(TASK, L(l), L(r))
#define _REF(n)    tlist_new_add2(TASK, _REF_, n)
#define _GET(n)    tlist_new_add2(TASK, _GET_, n)
#define _SET(n, v) tlist_new_add3(TASK, _SET_, n, v)

%}

 start = __ b:body __ !.   { $$ = b; }

  body = ts:stms           { $$ = tlist_prepend(TASK, L(ts), _BODY_); }

  stms = t:stm eol ts:stms { $$ = tlist_prepend(TASK, L(ts), t); }
       | t:stm             { $$ = tlist_new_add(TASK, t); }
       |                   { $$ = t_list_empty; }

   stm = var | assign | expr

   var = "var" _ "$" n:name _"="__ e:expr { $$ = tlist_new_add4(TASK, _ASSIGN_, _CALL1(_REF(tSYM("%var")), e), _RESULT_, n); }
       |         "$" n:name _"="__ e:expr { $$ = _CALL2(_REF(tSYM("%set")), _REF(n), e); }
assign =          as:anames _"="__ e:expr { $$ = tlist_prepend2(TASK, L(as), _ASSIGN_, e); }

anames =     n:name _","_ as:anames { $$ = tlist_prepend2(TASK, L(as), _RESULT_, n); }
       | "*" n:name                 { $$ = tlist_new_add2(TASK, _RESULT_REST_, n); }
       |     n:name                 { $$ = tlist_new_add2(TASK, _RESULT_, n); }

  expr = paren

fn = "{" __ as:fargs __ "=>" __ ts:stms __ "}" {
    tList* args = tlist_prepend(TASK, L(as), _ARGUMENTS_);
    $$ = tlist_prepend2(TASK, L(ts), _BODY_, args);
}

fargs = a:farg __","__ as:fargs { $$ = tlist_cat(TASK, L(a), L(as)); }
      | a:farg                  { $$ = tlist_add(TASK, a, _ARGS_); }
      | "*" n:name              { $$ = tlist_new_add2(TASK, _ARGS_REST_, n); }
      |                         { $$ = tlist_new_add(TASK, _ARGS_); }

farg = "&" n:name { $$ = tlist_new_add2(TASK, _ARG_LAZY_, n); }
     |     n:name { $$ = tlist_new_add2(TASK, _ARG_EVAL_, n); }

  tail = _"("__ as:cargs __")" t:tail {
           $$ = set_target(L(t), tlist_prepend2(TASK, L(as), _CALL_, null));
       }
       | _"."_ n:name _"("__ as:cargs __")" t:tail {
           $$ = set_target(L(t), _CAT(_CALL2(_SEND_, null, n), as));
       }
       | _"."_ n:name t:tail {
           $$ = set_target(L(t), _CALL2(_SEND_, null, n));
       }
       | _ {
           $$ = t_list_empty;
       }

 cargs = e:expr __","__ as:cargs       { $$ = tlist_prepend(TASK, L(as), e); }
       | e:expr                        { $$ = tlist_new_add(TASK, e) }
       |                               { $$ = t_list_empty; }

 paren = "("__ b:body __")" t:tail     { $$ = set_target(L(t), b); }
       | f:fn t:tail                   { $$ = set_target(L(t), f); }
       | v:value t:tail                { $$ = set_target(L(t), v); }

 value = lit | number | text | mut | ref | sym

   lit = "true"      { $$ = tTrue; }
       | "false"     { $$ = tFalse; }
       | "null"      { $$ = tNull; }
       | "undefined" { $$ = tUndef; }
       | "return"    { $$ = _CALL1(_REF(tSYM("return")), _REF(s_cc)); }
       | "goto"      { $$ = _CALL1(_REF(tSYM("goto")), _REF(s_cc)); }

   mut = "$" n:name  { $$ = _CALL1(_REF(tSYM("%get")), _REF(n)); }
   ref = n:name      { $$ = _REF(n); }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tINT(atoi(yytext)); }
  text = '"' < (!'"' .)* > '"'      { $$ = tTEXT(strdup(yytext)); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tSYM(strdup(yytext)); }

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
