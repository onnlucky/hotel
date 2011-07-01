%{
#include <stdio.h>
#include <string.h>

#include "tl.h"
#include "code.h"
#include "debug.h"

#include "platform.h"

#include "trace-off.h"

typedef struct ParseContext {
    tlTask* task;
    tlText* text;
    int at;
    int current_indent;
    int indents[100];
} ParseContext;

static inline int writesome(ParseContext* cx, char* buf, int len) {
    int towrite = tltext_size(cx->text) - cx->at;
    if (towrite <= 0) return 0;
    if (towrite < len) len = towrite;
    memcpy(buf, tltext_bytes(cx->text) + cx->at, len);
    cx->at += len;
    return len;
}

bool push_indent(void* data);
bool pop_indent(void* data);
bool check_indent(void* data);

#define YYSTYPE tlValue
#define YY_XTYPE ParseContext*
#define YY_INPUT(buf, len, max, cx) { len = writesome(cx, buf, max); }

void try_name(tlSym name, tlValue v) {
    if (tlactive_is(v)) v = tlvalue_from_active(v);
    if (tlcode_is(v)) {
        tlcode_set_name_(tlcode_as(v), name);
    }
    assert(tlsym_is(name));
}

tlValue map_activate(tlMap* map) {
    bool active = false;
    int i = 0;
    do {
        tlValue v = tlmap_value_iter(map, i);
        if (!v) break;
        if (tlactive_is(v)) { active = true; break; }
        if (tlcall_is(v)) { active = true; break; }
        i++;
    } while (true);
    if (active) return tlACTIVE(map);
    return map;
}
tlValue tlcall_value_iter(tlCall* call, int i);
tlCall* tlcall_value_iter_set_(tlCall* call, int i, tlValue v);
tlValue call_activate(tlValue in) {
    if (tlcall_is(in)) {
        tlCall* call = tlcall_as(in);
        for (int i = 0;; i++) {
            tlValue v = tlcall_value_iter(call, i);
            if (!v) break;
            tlValue v2 = call_activate(v);
            if (v != v2) tlcall_value_iter_set_(call, i, v2);
        }
        return tlACTIVE(call);
    }
    if (tlsend_is(in)) {
        tlSend* send = tlsend_as(in);
        for (int i = 0;; i++) {
            tlValue v = tlsend_value_iter(send, i);
            if (!v) break;
            tlValue v2 = call_activate(v);
            if (v != v2) tlsend_value_iter_set_(send, i, v2);
        }
        return tlACTIVE(send);
    }
    return in;
}
tlValue set_target(tlValue on, tlValue target) {
    if (!on) return target;
    if (tlcall_is(on)) {
        tlCall* call = tlcall_as(on);
        tlValue fn = tlcall_get_fn(call);
        if (!fn) {
            tlcall_set_fn_(call, target);
            return on;
        } else {
            set_target(fn, target);
            return on;
        }
    }
    if (tlsend_is(on)) {
        tlSend* send = tlsend_as(on);
        tlValue oop = tlsend_get_oop(send);
        if (!oop) {
            tlsend_set_oop_(send, target);
            return on;
        } else {
            set_target(oop, target);
            return on;
        }
    }
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
#define L(l) tllist_as(l)

tlValue tlcollect_new_(tlTask* task, tlList* list);

//#define YY_DEBUG
#define YY_STACK_SIZE 1024

%}

 start = __ b:body __ !.   { $$ = b; try_name(tlSYM("main"), b); }

  body = ts:stms           { $$ = tlcode_from(TASK, ts); }

  stms = t:stm (eos t2:stm { t = tllist_cat(TASK, L(t), L(t2)); }
               |eos)*      { $$ = t }
       |                   { $$ = tllist_empty(); }

bodynl = __ &{ push_indent(G) } ts:stmsnl { pop_indent(G); $$ = tlcode_from(TASK, ts); }
       | &{ pop_indent(G) }
stmsnl = _ &{ check_indent(G) } t:stm eos ts:stmsnl { $$ = tllist_cat(TASK, L(t), L(ts)); }
       | _ &{ check_indent(G) } t:stm               { $$ = L(t); }

   stm = singleassign | multiassign | noassign

#   var = "var" _ "$" n:name _"="__ e:pexpr { $$ = tllist_new_add4(TASK, _ASSIGN_, _CALL1(_REF(tlSYM("%var")), e), _RESULT_, n); }
#       |         "$" n:name _"="__ e:pexpr { $$ = _CALL2(_REF(tlSYM("%set")), _REF(n), e); }
#assign =          as:anames _"="__ e:pexpr { $$ = tllist_prepend2(TASK, L(as), _ASSIGN_, e); }

anames =     n:name _","_ as:anames { $$ = tllist_prepend(TASK, L(as), n); }
       #| "*" n:name                 { $$ = tllist_from1(TASK, n); }
       |     n:name                 { $$ = tllist_from1(TASK, n); }

singleassign = n:name    _"="__ e:pexpr { $$ = tllist_from(TASK, e, n, null); try_name(n, e); }
 multiassign = ns:anames _"="__ e:pexpr { $$ = tllist_from(TASK, e, tlcollect_new_(TASK, L(ns)), null); }
    noassign = e:selfapply  { $$ = tllist_from1(TASK, e); }
             | e:pexpr      { $$ = tllist_from1(TASK, e); }


    fn = "(" __ as:fargs __ ")"_"{" __ b:body __ "}" {
            tlcode_set_args_(TASK, tlcode_as(b), L(as));
            $$ = b;
        }
        | "{"__ b:body __ "}" { $$ = b; }

fargs = a:farg __","__ as:fargs { $$ = tllist_prepend(TASK, L(as), a); }
      | a:farg                  { $$ = tllist_from1(TASK, a); }
      |                         { $$ = tllist_empty(); }

farg = "&&" n:name { $$ = tllist_from2(TASK, n, tlCollectLazy); }
     | "**" n:name { $$ = tllist_from2(TASK, n, tlCollectEager); }
     | "&" n:name { $$ = tllist_from2(TASK, n, tlThunkNull); }
     |     n:name { $$ = tllist_from2(TASK, n, tlNull); }


  expr = e:op_log { $$ = call_activate(e); }

selfapply = n:name _ &eos {
    $$ = call_activate(tlcall_from_args(TASK, tlACTIVE(n), tllist_empty()));
}

 pexpr = "assert" _ !"(" < as:pcargs > {
            as = tllist_prepend(TASK, L(as), tltext_from_copy(TASK, yytext));
            $$ = call_activate(tlcall_from_args(TASK, tlACTIVE(tlSYM("assert")), as));
       }
       | fn:lookup _ ":" b:bodynl {
           tlcode_set_isblock_(b, true);
           as = tllist_from1(TASK, tlACTIVE(b));
           $$ = call_activate(tlcall_from_args(TASK, fn, as));
       }
       | fn:lookup _ !"(" as:pcargs _":"_ b:bodynl {
           tlcode_set_isblock_(b, true);
           as = tllist_add(TASK, L(as), tlACTIVE(b));
           $$ = call_activate(tlcall_from_args(TASK, fn, as));
       }
       | fn:lookup _ !"(" as:pcargs {
           trace("primary function call");
           $$ = call_activate(tlcall_from_args(TASK, fn, as));
       }
       | v:value _"."_ n:name _ !"(" as:pcargs _":"_ b:bodynl {
           fatal("primary send + bodynl");
       }
       | v:value _"."_ n:name _ !"(" as:pcargs {
           trace("primary send");
           $$ = call_activate(tlsend_from_args(TASK, v, n, as));
       }
       | expr

# // TODO fix below and add [] and such ...
       | v:value _"."_ n:name _ !"(" {
           // TODO here we want to "tail" more primary sends ...
           trace("primary send");
           //$$ = set_target(t, tlsend_from_args(TASK, null, n, tllist_empty()));
           $$ = tlsend_from_args(TASK, v, n, tllist_empty());
       }
       | expr


  tail = _"("__ as:cargs __")"_":"_ b:bodynl {
           trace("function call + bodynl");
           tlcode_set_isblock_(b, true);
           as = tllist_add(TASK, L(as), tlACTIVE(b));
           $$ = tlcall_from_args(TASK, null, as);
       }
       | _"("__ as:cargs __")" t:tail {
           trace("function call");
           $$ = set_target(t, tlcall_from_args(TASK, null, as));
       }
       | _"."_ n:name _"("__ as:cargs __")" t:tail {
           trace("method call()");
           $$ = set_target(t, tlsend_from_args(TASK, null, n, as));
       }
       | _"."_ n:name t:tail {
           trace("method call");
           $$ = set_target(t, tlsend_from_args(TASK, null, n, tllist_empty()));
       }
       | _"["__ e:expr __"]" t:tail {
           trace("array get call");
           $$ = set_target(t, tlsend_from_args(TASK, null, tlSYM("get"), as));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

pcargs = l:expr __","__          { l = tllist_from1(TASK, l); }
                (r:expr __","__  { l = tllist_add(TASK, L(l), r); }
                )*
                r:pexpr          { l = tllist_add(TASK, L(l), r); }
                                 { $$ = l; }
       | e:pexpr                 { $$ = tllist_from1(TASK, e) }

 cargs = l:expr                 { l = tllist_from1(TASK, l); }
                (__","__ r:expr { l = tllist_add(TASK, L(l), r); }
                )*              { $$ = l; }
       |                        { $$ = tllist_empty(); }

op_log = l:op_not _ ("or"  __ r:op_not { l = tlcall_from(TASK, tlACTIVE(tlSYM("or")), l, r, null); }
                  _ |"and" __ r:op_not { l = tlcall_from(TASK, tlACTIVE(tlSYM("and")), l, r, null); }
                  _ |"xor" __ r:op_not { l = tlcall_from(TASK, tlACTIVE(tlSYM("xor")), l, r, null); }
                    )*                 { $$ = l }
op_not = "not" __ r:op_cmp             { $$ = tlcall_from(TASK, tlACTIVE(tlSYM("not")), r, null); }
       | op_cmp
op_cmp = l:op_add _ ("<=" __ r:op_add { l = tlcall_from(TASK, tlACTIVE(tlSYM("lte")), l, r, null); }
                    |"<"  __ r:op_add { l = tlcall_from(TASK, tlACTIVE(tlSYM("lt")), l, r, null); }
                    |">"  __ r:op_add { l = tlcall_from(TASK, tlACTIVE(tlSYM("gt")), l, r, null); }
                    |">=" __ r:op_add { l = tlcall_from(TASK, tlACTIVE(tlSYM("gte")), l, r, null); }
                    |"==" __ r:op_add { l = tlcall_from(TASK, tlACTIVE(tlSYM("eq")), l, r, null); }
                    |"!=" __ r:op_add { l = tlcall_from(TASK, tlACTIVE(tlSYM("neq")), l, r, null); }
                    )*                { $$ = l; }
op_add = l:op_mul _ ("+" __ r:op_mul { l = tlcall_from(TASK, tlACTIVE(tlSYM("add")), l, r, null); }
                    |"-" __ r:op_mul { l = tlcall_from(TASK, tlACTIVE(tlSYM("sub")), l, r, null); }
                    )*               { $$ = l; }
op_mul = l:op_pow _ ("*" __ r:op_pow { l = tlcall_from(TASK, tlACTIVE(tlSYM("mul")), l, r, null); }
                    |"/" __ r:op_pow { l = tlcall_from(TASK, tlACTIVE(tlSYM("div")), l, r, null); }
                    |"%" __ r:op_pow { l = tlcall_from(TASK, tlACTIVE(tlSYM("mod")), l, r, null); }
                    )*               { $$ = l; }
op_pow = l:paren  _ ("^" __ r:paren  { l = tlcall_from(TASK, tlACTIVE(tlSYM("pow")), l, r, null); }
                    )*

 paren = "assert"_"("__ < as:cargs > __")" t:tail {
            as = tllist_prepend(TASK, L(as), tltext_from_copy(TASK, yytext));
            $$ = set_target(t, tlcall_from_args(TASK, tlACTIVE(tlSYM("assert")), as));
       }
       | f:fn t:tail                { $$ = set_target(t, tlACTIVE(f)); }
       | "("__ e:pexpr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body  __")" t:tail {
           $$ = set_target(t, tlcall_from_args(TASK, tlACTIVE(b), tllist_empty()));
       }
       | v:value t:tail             { $$ = set_target(t, v); }


   map = "["__ is:items __"]"   { $$ = map_activate(tlmap_from_list(TASK, L(is))); }
 items = i:item eom is:items    { $$ = tllist_cat(TASK, L(i), L(is)); }
       | i:item                 { $$ = i }
       |                        { $$ = tllist_empty(); }

  item = "+"_ n:name            { $$ = tlLIST2(TASK, n, tlTrue); }
       | "-"_ n:name            { $$ = tlLIST2(TASK, n, tlFalse); }
       | n:name _"="__ v:expr   { $$ = tlLIST2(TASK, n, v); }
       | v:expr                 { $$ = tlLIST2(TASK, tlNull, v); }


 value = lit | number | text | map | sym | lookup

   lit = "true"      { $$ = tlTrue; }
       | "false"     { $$ = tlFalse; }
       | "null"      { $$ = tlNull; }
       | "undefined" { $$ = tlUndefined; }
#       | "goto"      { $$ = _CALL1(_REF(s_goto), _REF(s_caller)); }

#   mut = "$" n:name  { $$ = _CALL1(_REF(tlSYM("%get")), _REF(n)); }
#   ref = n:name      { $$ = _REF(n); }
 lookup = n:name      { $$ = tlACTIVE(n); }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tlINT(atoi(yytext)); }

  text = '"' '"'          { $$ = tltext_empty(); }
       | '"'  t:stext '"' { $$ = t }
       | '"' ts:ctext '"' { $$ = tlcall_from_args(TASK, tlACTIVE(tlSYM("text_cat")), L(ts)); }

 stext = < (!"$" !"\"" .)+ > { $$ = tltext_from_take(TASK, unescape(yytext)); }
 ptext = "$("_ e:expr _")"   { $$ = e }
       | "$" l:lookup        { $$ = l }
 ctext = t:ptext ts:ctext    { $$ = tllist_prepend(TASK, ts, t); }
       | t:stext ts:ctext    { $$ = tllist_prepend(TASK, ts, t); }
       | t:ptext             { $$ = tllist_from1(TASK, t); }
       | t:stext             { $$ = tllist_from(TASK, t); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tlsym_from_copy(TASK, yytext); }

slcomment = "//" (!nl .)*
 icomment = "/*" (!"*/" .)* ("*/"|!.)
  comment = (slcomment nle | icomment)

      eos = _ (nl | ";" | slcomment nle) __
      eom = _ (nl | "," | slcomment nle) __
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

tlValue parse(tlText* text) {
    trace("\n----PARSING----\n%s----", tl_str(text));

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
    tlValue v = g.ss;
    yydeinit(&g);

    trace("\n----PARSED----");
    return v;
}

