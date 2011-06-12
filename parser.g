%{
#include <stdio.h>
#include <string.h>

#include "tl.h"
#include "body.h"
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

bool push_indent(void* data);
bool pop_indent(void* data);
bool check_indent(void* data);

#define YYSTYPE tValue
#define YY_XTYPE ParseContext*
#define YY_INPUT(buf, len, max, cx) { len = writesome(cx, buf, max); }

tValue set_target(tValue on, tValue target) {
    if (!on) return target;
    if (tcall_is(on)) {
        tCall* call = tcall_as(on);
        tValue fn = tcall_get_fn(call);
        if (!fn) {
            tcall_set_fn_(call, target);
            return on;
        } else {
            set_target(fn, target);
            return on;
        }
    }
#if 0
    if (tsend_is(on)) {
        tSend* send = tsend_as(on);
        tValue oop = tsend_get_oop(send);
        if (!oop) {
            tsend_set_oop_(send, target);
            return on;
        } else {
            set_target(oop, target);
            return on;
        }
    }
#endif
    assert(false);
    return null;
}
static char* unescape(const char* s) {
    char* t = strdup(s);
    int i = 0, j = 0;
    for (; s[i]; i++, j++) {
        if (s[i] == '\\') {
            i++;
            char c = s[i];
            switch (c) {
                case '\\': t[j] = '\\'; break;
                case 'n': t[j] = '\n'; break;
                case 'r': t[j] = '\r'; break;
                case 't': t[j] = '\t'; break;
                default: j--; break;
            }
        } else {
            t[j] = s[i];
        }
    }
    t[j] = 0;
    return t;
}

#define TASK yyxvar->task
#define L(l) tlist_as(l)

%}

 start = __ b:body __ !.   { $$ = b; }

  body = ts:stms           { $$ = tbody_from(TASK, ts); }

  stms = t:stm eol ts:stms { $$ = tlist_cat(TASK, L(t), L(ts)); }
       | t:stm             { $$ = L(t); }
       |                   { $$ = tlist_empty(); }

bodynl = __ &{ push_indent(G) } ts:stmsnl { pop_indent(G); $$ = ts; /*tlist_prepend(TASK, L(ts), _BODY_)*/ }
       | &{ pop_indent(G) }
stmsnl = _ &{ check_indent(G) } t:stm eol ts:stmsnl { $$ = tlist_prepend(TASK, L(ts), t); }
       | _ &{ check_indent(G) } t:stm               { $$ = tlist_from1(TASK, t); }

   stm = singleassign | multiassign | noassign
#   stm = var | assign | expr

#   var = "var" _ "$" n:name _"="__ e:expr { $$ = tlist_new_add4(TASK, _ASSIGN_, _CALL1(_REF(tSYM("%var")), e), _RESULT_, n); }
#       |         "$" n:name _"="__ e:expr { $$ = _CALL2(_REF(tSYM("%set")), _REF(n), e); }
#assign =          as:anames _"="__ e:expr { $$ = tlist_prepend2(TASK, L(as), _ASSIGN_, e); }

anames =     n:name _","_ as:anames { $$ = tlist_prepend(TASK, L(as), n); }
       #| "*" n:name                 { $$ = tlist_from1(TASK, n); }
       |     n:name                 { $$ = tlist_from1(TASK, n); }

singleassign = n:name    _"="__ e:expr { $$ = tlist_from(TASK, e, n, null); }
 multiassign = ns:anames _"="__ e:expr { $$ = tlist_from(TASK, e, ns, null); }
    noassign =                  e:expr { $$ = tlist_from1(TASK, e); }

  expr = op_log

fn = "(" __ as:fargs __ "=>" __ b:body __ ")" {
    tbody_set_args_(TASK, tbody_as(b), L(as));
    $$ = b;
}

fargs = a:farg __","__ as:fargs { $$ = tlist_prepend(TASK, L(as), a); }
      | a:farg                  { $$ = tlist_from1(TASK, a); }
      |                         { $$ = tlist_empty(); }

farg = "&&" n:name { $$ = tlist_from2(TASK, n, tCollectLazy); }
     | "**" n:name { $$ = tlist_from2(TASK, n, tCollectEager); }
     | "&" n:name { $$ = tlist_from2(TASK, n, tThunkNull); }
     |     n:name { $$ = tlist_from2(TASK, n, tNull); }

  tail = _"("__ as:cargs __")"_":"_ b:bodynl {
           fatal("function call + bodynl");
           //$$ = tlist_add(TASK, tlist_prepend2(TASK, L(as), _CALL_, null), b);
           //$$ = tlist_prepend2(TASK, L(as), _CALL_, null);
       }
       | _"("__ as:cargs __")" t:tail {
           trace("function call");
           $$ = set_target(t, tcall_from_args(TASK, null, as));
       }
       | _"."_ n:name _"("__ as:cargs __")" t:tail {
           fatal("method call")
           //$$ = set_target(L(t), _CAT(_CALL2(_SEND_, null, n), as));
       }
       | _"."_ n:name t:tail {
           fatal("method call")
           //$$ = set_target(L(t), _CALL2(_SEND_, null, n));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

 cargs = e:expr __","__ as:cargs   { $$ = tlist_prepend(TASK, L(as), e); }
       | e:expr                    { $$ = tlist_from1(TASK, e) }
       |                           { $$ = tlist_empty(); }


op_log = l:op_not _ ("or"  __ r:op_not { l = tcall_from(TASK, tACTIVE(tSYM("or")), l, r, null); }
                  _ |"and" __ r:op_not { l = tcall_from(TASK, tACTIVE(tSYM("and")), l, r, null); }
                  _ |"xor" __ r:op_not { l = tcall_from(TASK, tACTIVE(tSYM("xor")), l, r, null); }
                    )*                 { $$ = l }
op_not = "not" __ r:op_cmp             { $$ = tcall_from(TASK, tACTIVE(tSYM("not")), r, null); }
       | op_cmp
op_cmp = l:op_add _ ("<=" __ r:op_add { l = tcall_from(TASK, tACTIVE(tSYM("lte")), l, r, null); }
                    |"<"  __ r:op_add { l = tcall_from(TASK, tACTIVE(tSYM("lt")), l, r, null); }
                    |">"  __ r:op_add { l = tcall_from(TASK, tACTIVE(tSYM("gt")), l, r, null); }
                    |">=" __ r:op_add { l = tcall_from(TASK, tACTIVE(tSYM("gte")), l, r, null); }
                    |"==" __ r:op_add { l = tcall_from(TASK, tACTIVE(tSYM("eq")), l, r, null); }
                    |"!=" __ r:op_add { l = tcall_from(TASK, tACTIVE(tSYM("neq")), l, r, null); }
                    )*                { $$ = l; }
op_add = l:op_mul _ ("+" __ r:op_mul { l = tcall_from(TASK, tACTIVE(tSYM("add")), l, r, null); }
                    |"-" __ r:op_mul { l = tcall_from(TASK, tACTIVE(tSYM("sub")), l, r, null); }
                    )*               { $$ = l; }
op_mul = l:op_pow _ ("*" __ r:op_pow { l = tcall_from(TASK, tACTIVE(tSYM("mul")), l, r, null); }
                    |"/" __ r:op_pow { l = tcall_from(TASK, tACTIVE(tSYM("div")), l, r, null); }
                    |"%" __ r:op_pow { l = tcall_from(TASK, tACTIVE(tSYM("mod")), l, r, null); }
                    )*               { $$ = l; }
op_pow = l:paren  _ ("^" __ r:paren  { l = tcall_from(TASK, tACTIVE(tSYM("pow")), l, r, null); }
                    )*

 paren = "("__ e:expr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body __")" t:tail { $$ = set_target(t, tcall_from_args(TASK, tACTIVE(b), tlist_empty())); }
       | f:fn t:tail               { $$ = set_target(t, tACTIVE(f)); }
       | v:value t:tail            { $$ = set_target(t, v); }



   map = is:items               { $$ = tmap_from_list(TASK, L(is)); }
 items = i:item _","__ is:items { $$ = tlist_cat(TASK, L(i), L(is)); }
       | i:item                 { $$ = i }
       |                        { $$ = tNull; }

  item = "+"_ n:name            { $$ = tLIST2(TASK, n, tTrue); }
       | "-"_ n:name            { $$ = tLIST2(TASK, n, tFalse); }
       | n:name _"="__ v:value  { $$ = tLIST2(TASK, n, v); }
       | v:value                { $$ = tLIST2(TASK, tNull, v); }



# value = lit | number | text | mut | ref | sym
 value = lit | number | text | sym | lookup

   lit = "true"      { $$ = tTrue; }
       | "false"     { $$ = tFalse; }
       | "null"      { $$ = tNull; }
       | "undefined" { $$ = tUndefined; }
#       | "return"    { $$ = _CALL1(_REF(s_return), _REF(s_caller)); }
#       | "goto"      { $$ = _CALL1(_REF(s_goto), _REF(s_caller)); }

#   mut = "$" n:name  { $$ = _CALL1(_REF(tSYM("%get")), _REF(n)); }
#   ref = n:name      { $$ = _REF(n); }
 lookup = n:name      { $$ = tACTIVE(n); }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tINT(atoi(yytext)); }

  text = '"' '"'          { $$ = ttext_empty(); }
       | '"'  t:stext '"' { $$ = t }
       | '"' ts:ctext '"' { $$ = tcall_from_args(TASK, tACTIVE(tSYM("text_cat")), L(ts)); }

 stext = < (!"$" !"\"" .)+ > { $$ = ttext_from_take(TASK, unescape(yytext)); }
 ptext = "$("_ e:expr _")"   { $$ = e }
       | "$" l:lookup        { $$ = l }
 ctext = t:ptext ts:ctext    { $$ = tlist_prepend(TASK, ts, t); }
       | t:stext ts:ctext    { $$ = tlist_prepend(TASK, ts, t); }
       | t:ptext             { $$ = tlist_from1(TASK, t); }
       | t:stext             { $$ = tlist_from(TASK, t); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tsym_from_copy(TASK, yytext); }

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
        //trace("char=%c, pos=%d, indent=%d", c, G->pos, indent);
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
    //trace("NEW INDENT: %d", peek_indent(G));
    return true;
}
bool pop_indent(void* data) {
    GREG* G = (GREG*)data;
    G->data->current_indent--;
    assert(G->data->current_indent >= 0);
    //trace("NEW INDENT: %d", peek_indent(G));
    return false;
}
bool check_indent(void* data) {
    GREG* G = (GREG*)data;
    return peek_indent(G) == find_indent(G);
}

tValue parse(tText* text) {
    trace("\n----PARSING----\n%s----", t_str(text));

    ParseContext data;
    data.at = 0;
    data.text = text;
    data.current_indent = 0;
    data.indents[0] = 0;

    GREG g;
    yyinit(&g);
    g.data = &data;
    if (!yyparse(&g)) {
        fatal("ERROR: %d", g.pos + g.offset);
    }
    tValue v = g.ss;
    yydeinit(&g);

    trace("\n----PARSED----");
    return v;
}

