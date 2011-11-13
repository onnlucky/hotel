%{
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "tl.h"
#include "code.h"
#include "debug.h"

#include "platform.h"

#include "trace-off.h"

static tlValue sa_object_send;

typedef struct ParseContext {
    tlTask* task;
    tlText* text;
    int at;
    int current_indent;
    int indents[100];
} ParseContext;

static inline int writesome(ParseContext* cx, char* buf, int len) {
    int towrite = tlTextSize(cx->text) - cx->at;
    if (towrite <= 0) return 0;
    if (towrite < len) len = towrite;
    memcpy(buf, tlTextData(cx->text) + cx->at, len);
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
    if (tlActiveIs(v)) v = tl_value(v);
    if (tlcode_is(v)) {
        tlcode_set_name_(tlcode_as(v), name);
    }
    assert(tlSymIs(name));
}

tlValue map_activate(tlMap* map) {
    int i = 0;
    int argc = 0;
    for (int i = 0;; i++) {
        tlValue v = tlMapValueIter(map, i);
        if (!v) break;
        if (tlActiveIs(v) || tlcall_is(v)) argc++;
    }
    if (!argc) return map;
    tlCall* call = tlcall_new(null, argc + 1, null);
    tlcall_fn_set_(call, tl_active(tlSYM("_Map_clone")));
    tlcall_arg_set_(call, 0, map);
    argc = 1;
    for (int i = 0;; i++) {
        tlValue v = tlMapValueIter(map, i);
        if (!v) break;
        if (tlActiveIs(v) || tlcall_is(v)) {
            tlMapValueIterSet_(map, i, null);
            tlcall_arg_set_(call, argc++, v);
        }
    }
    return call;
}
tlValue list_activate(tlList* list) {
    int size = tlListSize(list);
    int argc = 0;
    for (int i = 0; i < size; i++) {
        tlValue v = tlListGet(list, i);
        if (tlActiveIs(v) || tlcall_is(v)) argc++;
    }
    if (!argc) return list;
    tlCall* call = tlcall_new(null, argc + 1, null);
    tlcall_fn_set_(call, tl_active(tlSYM("_List_clone")));
    tlcall_arg_set_(call, 0, list);
    argc = 1;
    for (int i = 0; i < size; i++) {
        tlValue v = tlListGet(list, i);
        if (tlActiveIs(v) || tlcall_is(v)) {
            tlListSet_(list, i, null);
            tlcall_arg_set_(call, argc++, v);
        }
    }
    return call;
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
        return tl_active(call);
    }
    return in;
}
tlValue set_target(tlValue on, tlValue target) {
    if (!on) return target;
    if (tlcall_is(on)) {
        tlCall* call = tlcall_as(on);
        tlValue fn = tlcall_fn(call);

        if (fn == sa_object_send) {
            tlValue oop = tlcall_arg(call, 0);
            if (!oop) {
                tlcall_arg_set_(call, 0, target);
                return on;
            } else {
                set_target(oop, target);
                return on;
            }
        }

        if (!fn) {
            tlcall_fn_set_(call, target);
            return on;
        } else {
            set_target(fn, target);
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
#define L(l) tlListAs(l)

tlValue tlcollect_new_(tlTask* task, tlList* list);

//#define YY_DEBUG
#define YY_STACK_SIZE 1024

%}

 start = __ b:body __ !.   { $$ = b; try_name(tlSYM("main"), b); }

  body = ts:stms           { $$ = tlcode_from(TASK, ts); }

  stms = t:stm (eos t2:stm { t = tlListCat(TASK, L(t), L(t2)); }
               |eos)*      { $$ = t }
       |                   { $$ = tlListEmpty(); }

stmsnl =  t:stm ssep ts:stmsnl { $$ = tlListCat(TASK, L(t), L(ts)); }
       |  t:stm                { $$ = L(t); }

   stm = varassign | singleassign | multiassign | noassign

anames =     n:name _","_ as:anames { $$ = tlListPrepend(TASK, L(as), n); }
       #| "*" n:name                 { $$ = tlListFrom1(TASK, n); }
       |     n:name                 { $$ = tlListFrom1(TASK, n); }

   varassign = "var"__"$" n:name __"="__ e:bpexpr {
                 $$ = tlListFrom(TASK, call_activate(
                             tlcall_from(TASK, tl_active(tlSYM("_Var_new")), e, null)
                 ), n, null);
             }
             | "$" n:name _"="__ e:bpexpr {
                 $$ = tlListFrom1(TASK, call_activate(
                             tlcall_from(TASK, tl_active(tlSYM("_var_set")), tl_active(n), e, null)
                 ));
             }

singleassign = n:name    _"="__ e:fn    { $$ = tlListFrom(TASK, tl_active(e), n, null); try_name(n, e); }
             | n:name    _"="__ e:bpexpr { $$ = tlListFrom(TASK, e, n, null); try_name(n, e); }
 multiassign = ns:anames _"="__ e:bpexpr { $$ = tlListFrom(TASK, e, tlcollect_new_(TASK, L(ns)), null); }
    noassign = e:selfapply  { $$ = tlListFrom1(TASK, e); }
             | e:bpexpr     { $$ = tlListFrom1(TASK, e); }


 block = b:fn {
           tlcode_set_isblock_(b, true);
           $$ = b;
       }
       | "("__ b:body __ ")" {
           tlcode_set_isblock_(b, true);
           $$ = b;
       }
       | ts:stmsnl &ssepend {
           b = tlcode_from(TASK, ts);
           tlcode_set_isblock_(b, true);
           $$ = b;
       }

    fn = "(" __ as:fargs __"->"__ b:body __ ")" {
           tlcode_set_args_(TASK, tlcode_as(b), L(as));
           $$ = b;
       }
       | as:fargs _"->"_ ts:stmsnl &ssepend {
           b = tlcode_from(TASK, ts);
           tlcode_set_args_(TASK, tlcode_as(b), L(as));
           $$ = b;
       }

fargs = a:farg __","__ as:fargs { $$ = tlListPrepend(TASK, L(as), a); }
      | a:farg                  { $$ = tlListFrom1(TASK, a); }
      |                         { $$ = tlListEmpty(); }

farg = "&&" n:name { $$ = tlListFrom2(TASK, n, tlCollectLazy); }
     | "**" n:name { $$ = tlListFrom2(TASK, n, tlCollectEager); }
     | "&" n:name { $$ = tlListFrom2(TASK, n, tlThunkNull); }
     |     n:name { $$ = tlListFrom2(TASK, n, tlNull); }


  expr = "!" b:block {
            $$ = tlcall_from_list(TASK,
                        tl_active(tlSYM("_Task_new")), tlListFrom2(TASK, tlNull, tl_active(b)));
       }
       | op_log

selfapply = n:name _ &eosfull {
    $$ = call_activate(tlcall_from_list(TASK, tl_active(n), tlListEmpty()));
}

bpexpr = v:value t:ptail _":"_ b:block {
           $$ = call_activate(tlcall_add_block(TASK, set_target(t, v), tl_active(b)));
       }
       | e:expr _":"_ b:block {
           $$ = call_activate(tlcall_add_block(TASK, e, tl_active(b)));
       }
       | pexpr

 pexpr = "assert" _!"(" < as:pcargs > &peosfull {
            as = tlListAppend2(TASK, L(as), tlSYM("text"), tlTextFromCopy(TASK, yytext, 0));
            $$ = call_activate(tlcall_from_list(TASK, tl_active(tlSYM("assert")), as));
       }
       | "!" b:block {
            $$ = call_activate(tlcall_from_list(TASK,
                        tl_active(tlSYM("_Task_new")), tlListFrom2(TASK, tlNull, tl_active(b))));
       }
       | v:value t:ptail &peosfull { $$ = call_activate(set_target(t, v)); }
       | e:expr                   { $$ = call_activate(e); }

# // TODO add [] and oop.method: and oop.method arg1: ... etc
 ptail = _!"(" as:pcargs {
           trace("primary args");
           $$ = tlcall_from_list(TASK, null, as);
       }
       | _"."_ n:name _"("__ as:cargs __")" t:ptail {
           trace("primary method + args()");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, n, as));
       }
       | _"."_ n:name _ as:pcargs {
           trace("primary method + args");
           $$ = tlcall_send_from_list(TASK, sa_object_send, null, n, as);
       }
       | _"."_ n:name t:ptail {
           trace("primary method");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, n, tlListEmpty()));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

  tail = _"("__ as:cargs __")" t:tail {
           trace("function args");
           $$ = set_target(t, tlcall_from_list(TASK, null, as));
       }
       | _"."_ n:name _"("__ as:cargs __")" t:tail {
           trace("method args()");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, n, as));
       }
       | _"."_ n:name t:tail {
           trace("method");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, n, tlListEmpty()));
       }
       | _"["__ e:expr __"]" t:tail {
           trace("array get");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, tlSYM("get"), as));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

pcargs = l:carg __","__
                (r:carg __","__  { l = tlListCat(TASK, L(l), r); }
                )*
                r:pcarg          { l = tlListCat(TASK, L(l), r); }
                                 { $$ = l; }
       | r:pcarg                 { $$ = r; }

 cargs = l:carg
                (__","__ r:carg { l = tlListCat(TASK, L(l), r); }
                )*              { $$ = l; }
       |                        { $$ = tlListEmpty(); }

 pcarg = "+"_ n:name            { $$ = tlListFrom2(TASK, n, tlTrue); }
       | "-"_ n:name            { $$ = tlListFrom2(TASK, n, tlFalse); }
       | n:name _"="__ v:expr   { $$ = tlListFrom2(TASK, n, v); }
       | v:pexpr                { $$ = tlListFrom2(TASK, tlNull, v); }

  carg = "+"_ n:name            { $$ = tlListFrom2(TASK, n, tlTrue); }
       | "-"_ n:name            { $$ = tlListFrom2(TASK, n, tlFalse); }
       | n:name _"="__ v:expr   { $$ = tlListFrom2(TASK, n, v); }
       | v:expr                 { $$ = tlListFrom2(TASK, tlNull, v); }


op_log = l:op_not _ ("or"  __ r:op_not { l = tlcall_from(TASK, tl_active(tlSYM("or")), l, r, null); }
                  _ |"and" __ r:op_not { l = tlcall_from(TASK, tl_active(tlSYM("and")), l, r, null); }
                  _ |"xor" __ r:op_not { l = tlcall_from(TASK, tl_active(tlSYM("xor")), l, r, null); }
                    )*                 { $$ = l }
op_not = "not" __ r:op_cmp             { $$ = tlcall_from(TASK, tl_active(tlSYM("not")), r, null); }
       | op_cmp
op_cmp = l:op_add _ ("<=" __ r:op_add { l = tlcall_from(TASK, tl_active(tlSYM("lte")), l, r, null); }
                    |"<"  __ r:op_add { l = tlcall_from(TASK, tl_active(tlSYM("lt")), l, r, null); }
                    |">"  __ r:op_add { l = tlcall_from(TASK, tl_active(tlSYM("gt")), l, r, null); }
                    |">=" __ r:op_add { l = tlcall_from(TASK, tl_active(tlSYM("gte")), l, r, null); }
                    |"==" __ r:op_add { l = tlcall_from(TASK, tl_active(tlSYM("eq")), l, r, null); }
                    |"!=" __ r:op_add { l = tlcall_from(TASK, tl_active(tlSYM("neq")), l, r, null); }
                    )*                { $$ = l; }
op_add = l:op_mul _ ("+" __ r:op_mul { l = tlcall_from(TASK, tl_active(tlSYM("add")), l, r, null); }
                    |"-" __ r:op_mul { l = tlcall_from(TASK, tl_active(tlSYM("sub")), l, r, null); }
                    )*               { $$ = l; }
op_mul = l:op_pow _ ("*" __ r:op_pow { l = tlcall_from(TASK, tl_active(tlSYM("mul")), l, r, null); }
                    |"/" __ r:op_pow { l = tlcall_from(TASK, tl_active(tlSYM("div")), l, r, null); }
                    |"%" __ r:op_pow { l = tlcall_from(TASK, tl_active(tlSYM("mod")), l, r, null); }
                    )*               { $$ = l; }
op_pow = l:paren  _ ("^" __ r:paren  { l = tlcall_from(TASK, tl_active(tlSYM("pow")), l, r, null); }
                    )*

 paren = "assert"_"("__ < as:cargs > __")" t:tail {
            as = tlListPrepend2(TASK, L(as), tlSYM("text"), tlTextFromCopy(TASK, yytext, 0));
            $$ = set_target(t, tlcall_from_list(TASK, tl_active(tlSYM("assert")), as));
       }
       | f:fn t:tail                { $$ = set_target(t, tl_active(f)); }
       | "("__ e:pexpr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body  __")" t:tail {
           tlcode_set_isblock_(b, true);
           $$ = set_target(t, tlcall_from_list(TASK, tl_active(b), tlListEmpty()));
       }
       | v:value t:tail             { $$ = set_target(t, v); }


object = "{"__ is:items __"}"  { $$ = map_activate(tlMapToObject_(tlMapFromPairs(TASK, L(is)))); }
       | "{"__"}"              { $$ = map_activate(tlMapToObject_(tlMapFromPairs(TASK, L(is)))); }
   map = "["__ is:items __"]"  { $$ = map_activate(tlMapFromPairs(TASK, L(is))); }
       | "["__":"__"]"         { $$ = map_activate(tlMapEmpty()); }
 items = i:item eom is:items   { $$ = tlListPrepend(TASK, L(is), i); }
       | i:item                { $$ = tlListFrom1(TASK, i) }
  item = n:name _":"__ v:expr  { $$ = tlListFrom2(TASK, n, v); }

  list = "["__ is:litems __"]" { $$ = list_activate(L(is)); }
       | "["__"]"              { $$ = list_activate(tlListEmpty()); }
litems = v:expr eom is:litems  { $$ = tlListPrepend(TASK, L(is), v); }
       | v:expr                { $$ = tlListFrom1(TASK, v); }

 value = lit | number | text | object | map | list | sym | varref | lookup

   lit = "true"      { $$ = tlTrue; }
       | "false"     { $$ = tlFalse; }
       | "null"      { $$ = tlNull; }
       | "undefined" { $$ = tlUndefined; }

 varref = "$" n:name  { $$ = tlcall_from(TASK, tl_active(tlSYM("_var_get")), tl_active(n), null); }
 lookup = n:name      { $$ = tl_active(n); }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tlINT(atoi(yytext)); }

  text = '"' '"'          { $$ = tlTextEmpty(); }
       | '"'  t:stext '"' { $$ = t }
       | '"' ts:ctext '"' { $$ = tlcall_from_list(TASK, tl_active(tlSYM("_Text_cat")), L(ts)); }

 stext = < (!"$" !"\"" .)+ > { $$ = tlTextFromTake(TASK, unescape(yytext), 0); }
 ptext = "$("_ e:expr _")"   { $$ = e }
       | "$" l:lookup        { $$ = l }
 ctext = t:ptext ts:ctext    { $$ = tlListPrepend2(TASK, ts, tlNull, t); }
       | t:stext ts:ctext    { $$ = tlListPrepend2(TASK, ts, tlNull, t); }
       | t:ptext             { $$ = tlListFrom2(TASK, tlNull, t); }
       | t:stext             { $$ = tlListFrom2(TASK, tlNull, t); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tlSymFromCopy(TASK, yytext, 0); }

slcomment = "//" (!nl .)*
 icomment = "/*" (!"*/" .)* ("*/"|!.)
  comment = (slcomment nle | icomment)

     ssep = _ ";" _
  ssepend = _ (nl | "}" | ")" | "]" | slcomment nle | !.) __

      eos = _ (nl | ";" | slcomment nle) __
  eosfull = _ (nl | ";" | "}" | ")" | "]" | slcomment nle | !.) __
 peosfull = _ (nl | ":" | ";" | "}" | ")" | "]" | slcomment nle | !.) __
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

tlValue tl_parse(tlTask* task, tlText* text) {
    trace("\n----PARSING----\n%s----", tl_str(text));

    if (!sa_object_send) sa_object_send = tl_active(tlSYM("_object_send"));

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
    //debugcode(tlcode_as(v));
    return v;
}

