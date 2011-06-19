%{
#include <stdio.h>
#include <string.h>

#include "tl.h"
#include "code.h"
#include "debug.h"

#include "platform.h"

#include "trace-off.h"

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

void try_name(tSym name, tValue v) {
    if (tactive_is(v)) v = tvalue_from_active(v);
    if (tcode_is(v)) {
        tcode_set_name_(tcode_as(v), name);
    }
    assert(tsym_is(name));
}

tValue map_activate(tMap* map) {
    bool active = false;
    int i = 0;
    do {
        tValue v = tmap_value_iter(map, i);
        if (!v) break;
        if (tactive_is(v)) { active = true; break; }
        if (tcall_is(v)) { active = true; break; }
        i++;
    } while (true);
    if (active) return tACTIVE(map);
    return map;
}
tValue tcall_value_iter(tCall* call, int i);
tCall* tcall_value_iter_set_(tCall* call, int i, tValue v);
tValue call_activate(tValue* in) {
    if (tcall_is(in)) {
        tCall* call = tcall_as(in);
        for (int i = 0;; i++) {
            tValue v = tcall_value_iter(call, i);
            if (!v) break;
            tValue v2 = call_activate(v);
            if (v != v2) tcall_value_iter_set_(call, i, v2);
        }
        return tACTIVE(call);
    }
    return in;
}
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

tValue tcollect_new_(tTask* task, tList* list);

%}

 start = __ b:body __ !.   { $$ = b; try_name(tSYM("main"), b); }

  body = ts:stms           { $$ = tcode_from(TASK, ts); }

  stms = t:stm eos ts:stms { $$ = tlist_cat(TASK, L(t), L(ts)); }
       | t:stm             { $$ = L(t); }
       |                   { $$ = tlist_empty(); }

bodynl = __ &{ push_indent(G) } ts:stmsnl { pop_indent(G); $$ = tcode_from(TASK, ts); }
       | &{ pop_indent(G) }
stmsnl = _ &{ check_indent(G) } t:stm eos ts:stmsnl { $$ = tlist_cat(TASK, L(t), L(ts)); }
       | _ &{ check_indent(G) } t:stm               { $$ = L(t); }

   stm = singleassign | multiassign | noassign

#   var = "var" _ "$" n:name _"="__ e:pexpr { $$ = tlist_new_add4(TASK, _ASSIGN_, _CALL1(_REF(tSYM("%var")), e), _RESULT_, n); }
#       |         "$" n:name _"="__ e:pexpr { $$ = _CALL2(_REF(tSYM("%set")), _REF(n), e); }
#assign =          as:anames _"="__ e:pexpr { $$ = tlist_prepend2(TASK, L(as), _ASSIGN_, e); }

anames =     n:name _","_ as:anames { $$ = tlist_prepend(TASK, L(as), n); }
       #| "*" n:name                 { $$ = tlist_from1(TASK, n); }
       |     n:name                 { $$ = tlist_from1(TASK, n); }

singleassign = n:name    _"="__ e:pexpr { $$ = tlist_from(TASK, e, n, null); try_name(n, e); }
 multiassign = ns:anames _"="__ e:pexpr { $$ = tlist_from(TASK, e, tcollect_new_(TASK, L(ns)), null); }
    noassign =                  e:pexpr { $$ = tlist_from1(TASK, e); }


    fn = "(" __ as:fargs __ ")"_"{" __ b:body __ "}" {
            tcode_set_args_(TASK, tcode_as(b), L(as));
            $$ = b;
        }
        | "{"__ b:body __ "}" { $$ = b; }

fargs = a:farg __","__ as:fargs { $$ = tlist_prepend(TASK, L(as), a); }
      | a:farg                  { $$ = tlist_from1(TASK, a); }
      |                         { $$ = tlist_empty(); }

farg = "&&" n:name { $$ = tlist_from2(TASK, n, tCollectLazy); }
     | "**" n:name { $$ = tlist_from2(TASK, n, tCollectEager); }
     | "&" n:name { $$ = tlist_from2(TASK, n, tThunkNull); }
     |     n:name { $$ = tlist_from2(TASK, n, tNull); }


  expr = e:op_log { $$ = call_activate(e); }

 pexpr = "assert" _ !"(" < as:pcargs > {
            as = tlist_prepend(TASK, L(as), ttext_from_copy(TASK, yytext));
            $$ = call_activate((tValue)tcall_from_args(TASK, tACTIVE(tSYM("assert")), as));
       }
       | fn:lookup _ ":" b:bodynl {
           tcode_set_isblock_(b, true);
           as = tlist_from1(TASK, tACTIVE(b));
           $$ = call_activate((tValue)tcall_from_args(TASK, fn, as));
       }
       | fn:lookup _ !"(" as:pcargs _":"_ b:bodynl {
           tcode_set_isblock_(b, true);
           as = tlist_add(TASK, L(as), tACTIVE(b));
           $$ = call_activate((tValue)tcall_from_args(TASK, fn, as));
       }
       | fn:lookup _ !"(" as:pcargs {
           trace("primary function call");
           $$ = call_activate((tValue)tcall_from_args(TASK, fn, as));
       }
       | v:value _"."_ n:name _ !"(" as:pcargs _":"_ b:bodynl {
           fatal("primary send + bodynl");
       }
       | v:value _"."_ n:name _ !"(" as:pcargs {
           fatal("primary send");
       }
       | expr


  tail = _"("__ as:cargs __")"_":"_ b:bodynl {
           trace("function call + bodynl");
           tcode_set_isblock_(b, true);
           as = tlist_add(TASK, L(as), tACTIVE(b));
           $$ = tcall_from_args(TASK, null, as);
       }
       | _"("__ as:cargs __")" t:tail {
           trace("function call");
           $$ = set_target(t, tcall_from_args(TASK, null, as));
       }
       | _"."_ n:name _"("__ as:cargs __")" t:tail {
           fatal("method call");
           //$$ = set_target(L(t), _CAT(_CALL2(_SEND_, null, n), as));
       }
       | _"."_ n:name t:tail {
           fatal("method call");
           //$$ = set_target(L(t), _CALL2(_SEND_, null, n));
       }
       | _"["__ e:expr __"]" t:tail {
           fatal("array get call");
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

pcargs = e:expr __","__ as:pcargs   { $$ = tlist_prepend(TASK, L(as), e); }
       | e:pexpr                    { $$ = tlist_from1(TASK, e) }

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

 paren = "assert"_"("__ < as:cargs > __")" t:tail {
            as = tlist_prepend(TASK, L(as), ttext_from_copy(TASK, yytext));
            $$ = set_target(t, tcall_from_args(TASK, tACTIVE(tSYM("assert")), as));
       }
       | f:fn t:tail                { $$ = set_target(t, tACTIVE(f)); }
       | "("__ e:pexpr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body  __")" t:tail {
           $$ = set_target(t, tcall_from_args(TASK, tACTIVE(b), tlist_empty()));
       }
       | v:value t:tail             { $$ = set_target(t, v); }


   map = "["__ is:items __"]"   { $$ = map_activate(tmap_from_list(TASK, L(is))); }
 items = i:item eom is:items    { $$ = tlist_cat(TASK, L(i), L(is)); }
       | i:item                 { $$ = i }
       |                        { $$ = tlist_empty(); }

  item = "+"_ n:name            { $$ = tLIST2(TASK, n, tTrue); }
       | "-"_ n:name            { $$ = tLIST2(TASK, n, tFalse); }
       | n:name _"="__ v:expr   { $$ = tLIST2(TASK, n, v); }
       | v:expr                 { $$ = tLIST2(TASK, tNull, v); }


 value = lit | number | text | map | sym | lookup

   lit = "true"      { $$ = tTrue; }
       | "false"     { $$ = tFalse; }
       | "null"      { $$ = tNull; }
       | "undefined" { $$ = tUndefined; }
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
 icomment = "/*" (!"*/" .)* ("*/"|!.)
  comment = (slcomment nle | icomment)

      eos = _ (nle | ";" | slcomment nle) __
      eom = _ (nle | "," | slcomment nle) __
       nl = "\n" | "\r\n" | "\r"
      nle = "\n" | "\r\n" | "\r" | !.
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

