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
    if (tlactive_is(v)) v = tlvalue_from_active(v);
    if (tlcode_is(v)) {
        tlcode_set_name_(tlcode_as(v), name);
    }
    assert(tlsym_is(name));
}

tlValue map_activate(tlMap* map) {
    int i = 0;
    int argc = 0;
    for (int i = 0;; i++) {
        tlValue v = tlmap_value_iter(map, i);
        if (!v) break;
        if (tlactive_is(v) || tlcall_is(v)) argc++;
    }
    if (!argc) return map;
    tlCall* call = tlcall_new(null, argc + 1, null);
    tlcall_fn_set_(call, tlACTIVE(tlSYM("_map_clone")));
    tlcall_arg_set_(call, 0, map);
    argc = 1;
    for (int i = 0;; i++) {
        tlValue v = tlmap_value_iter(map, i);
        if (!v) break;
        if (tlactive_is(v) || tlcall_is(v)) {
            tlmap_value_iter_set_(map, i, null);
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
        if (tlactive_is(v) || tlcall_is(v)) argc++;
    }
    if (!argc) return list;
    tlCall* call = tlcall_new(null, argc + 1, null);
    tlcall_fn_set_(call, tlACTIVE(tlSYM("_list_clone")));
    tlcall_arg_set_(call, 0, list);
    argc = 1;
    for (int i = 0; i < size; i++) {
        tlValue v = tlListGet(list, i);
        if (tlactive_is(v) || tlcall_is(v)) {
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
        return tlACTIVE(call);
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
       #| "*" n:name                 { $$ = tlListNewFrom1(TASK, n); }
       |     n:name                 { $$ = tlListNewFrom1(TASK, n); }

   varassign = "var"__"$" n:name __"="__ e:pexpr {
                 $$ = tlListNewFrom(TASK, call_activate(
                             tlcall_from(TASK, tlACTIVE(tlSYM("_Var_new")), e, null)
                 ), n, null);
             }
             | "$" n:name _"="__ e:pexpr {
                 $$ = tlListNewFrom1(TASK, call_activate(
                             tlcall_from(TASK, tlACTIVE(tlSYM("_var_set")), tlACTIVE(n), e, null)
                 ));
             }

singleassign = n:name    _"="__ e:fn    { $$ = tlListNewFrom(TASK, tlACTIVE(e), n, null); try_name(n, e); }
             | n:name    _"="__ e:pexpr { $$ = tlListNewFrom(TASK, e, n, null); try_name(n, e); }
 multiassign = ns:anames _"="__ e:pexpr { $$ = tlListNewFrom(TASK, e, tlcollect_new_(TASK, L(ns)), null); }
    noassign = e:selfapply  { $$ = tlListNewFrom1(TASK, e); }
             | e:pexpr      { $$ = tlListNewFrom1(TASK, e); }


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
      | a:farg                  { $$ = tlListNewFrom1(TASK, a); }
      |                         { $$ = tlListEmpty(); }

farg = "&&" n:name { $$ = tlListNewFrom2(TASK, n, tlCollectLazy); }
     | "**" n:name { $$ = tlListNewFrom2(TASK, n, tlCollectEager); }
     | "&" n:name { $$ = tlListNewFrom2(TASK, n, tlThunkNull); }
     |     n:name { $$ = tlListNewFrom2(TASK, n, tlNull); }


  expr = "!" b:block {
            $$ = call_activate(tlcall_from_list(TASK,
                        tlACTIVE(tlSYM("_Task_new")), tlListNewFrom2(TASK, tlNull, tlACTIVE(b))));
       }
       | e:op_log { $$ = call_activate(e); }

selfapply = n:name _ &eosfull {
    $$ = call_activate(tlcall_from_list(TASK, tlACTIVE(n), tlListEmpty()));
}

 pexpr = "assert" _!"(" < as:pcargs > &eosfull {
            as = tlListAppend2(TASK, L(as), tlSYM("text"), tlTextNewCopy(TASK, yytext));
            $$ = call_activate(tlcall_from_list(TASK, tlACTIVE(tlSYM("assert")), as));
       }
       | "!" b:block {
            $$ = call_activate(tlcall_from_list(TASK,
                        tlACTIVE(tlSYM("_Task_new")), tlListNewFrom2(TASK, tlNull, tlACTIVE(b))));
       }
       | v:value t:ptail &eosfull {
           $$ = call_activate(set_target(t, v));
       }
       | expr

# // TODO add [] and oop.method: and oop.method arg1: ... etc
 ptail = _!"(" as:pcargs _":"_ b:block {
           trace("primary args + body");
           as = tlListAppend2(TASK, L(as), tlSYM("block"), tlACTIVE(b));
           $$ = tlcall_from_list(TASK, null, as);
       }
       | _":"_ b:block {
           trace("primary block");
           $$ = tlcall_from_list(TASK, null, tlListNewFrom2(TASK, tlSYM("block"), tlACTIVE(b)));
       }
       | _!"(" as:pcargs {
           trace("primary args");
           $$ = tlcall_from_list(TASK, null, as);
       }
       | _"."_ n:name _"("__ as:cargs __")" t:ptail {
           trace("primary method + args()");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, n, as));
       }
       | _"."_ n:name _ as:cargs _":"_ b:block {
           trace("primary method + args + body");
           as = tlListAppend2(TASK, L(as), tlSYM("block"), tlACTIVE(b));
           $$ = tlcall_send_from_list(TASK, sa_object_send, null, n, as);
       }
       | _"."_ n:name _ as:pcargs {
           trace("primary method + args");
           $$ = tlcall_send_from_list(TASK, sa_object_send, null, n, as);
       }
       | _"."_ n:name _":"_ b:block {
           trace("primary method + body");
           tlList* _as = tlListNewFrom2(TASK, tlSYM("block"), tlACTIVE(b));
           $$ = tlcall_send_from_list(TASK, sa_object_send, null, n, _as);
       }
       | _"."_ n:name t:ptail {
           trace("primary method");
           $$ = set_target(t, tlcall_send_from_list(TASK, sa_object_send, null, n, tlListEmpty()));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

  tail = _"("__ as:cargs __")"_":"_ b:block {
           trace("function args + block");
           as = tlListAppend2(TASK, L(as), tlSYM("block"), tlACTIVE(b));
           $$ = tlcall_from_list(TASK, null, as);
       }
       | _"("__ as:cargs __")" t:tail {
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

 pcarg = "+"_ n:name            { $$ = tlListNewFrom2(TASK, n, tlTrue); }
       | "-"_ n:name            { $$ = tlListNewFrom2(TASK, n, tlFalse); }
       | n:name _"="__ v:expr   { $$ = tlListNewFrom2(TASK, n, v); }
       | v:pexpr                { $$ = tlListNewFrom2(TASK, tlNull, v); }

  carg = "+"_ n:name            { $$ = tlListNewFrom2(TASK, n, tlTrue); }
       | "-"_ n:name            { $$ = tlListNewFrom2(TASK, n, tlFalse); }
       | n:name _"="__ v:expr   { $$ = tlListNewFrom2(TASK, n, v); }
       | v:expr                 { $$ = tlListNewFrom2(TASK, tlNull, v); }


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
            as = tlListPrepend2(TASK, L(as), tlSYM("text"), tlTextNewCopy(TASK, yytext));
            $$ = set_target(t, tlcall_from_list(TASK, tlACTIVE(tlSYM("assert")), as));
       }
       | f:fn t:tail                { $$ = set_target(t, tlACTIVE(f)); }
       | "("__ e:pexpr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body  __")" t:tail {
           tlcode_set_isblock_(b, true);
           $$ = set_target(t, tlcall_from_list(TASK, tlACTIVE(b), tlListEmpty()));
       }
       | v:value t:tail             { $$ = set_target(t, v); }


object = "{"__ is:items __"}"  { $$ = map_activate(tlMapToObject_(tlmap_from_pairs(TASK, L(is)))); }
       | "{"__"}"              { $$ = map_activate(tlMapToObject_(tlmap_from_pairs(TASK, L(is)))); }
   map = "["__ is:items __"]"  { $$ = map_activate(tlmap_from_pairs(TASK, L(is))); }
       | "["__":"__"]"         { $$ = map_activate(tlmap_empty()); }
 items = i:item eom is:items   { $$ = tlListPrepend(TASK, L(is), i); }
       | i:item                { $$ = tlListNewFrom1(TASK, i) }
  item = n:name _":"__ v:expr  { $$ = tlListNewFrom2(TASK, n, v); }

  list = "["__ is:litems __"]" { $$ = list_activate(L(is)); }
       | "["__"]"              { $$ = list_activate(tlListEmpty()); }
litems = v:expr eom is:litems  { $$ = tlListPrepend(TASK, L(is), v); }
       | v:expr                { $$ = tlListNewFrom1(TASK, v); }

 value = lit | number | text | object | map | list | sym | varref | lookup

   lit = "true"      { $$ = tlTrue; }
       | "false"     { $$ = tlFalse; }
       | "null"      { $$ = tlNull; }
       | "undefined" { $$ = tlUndefined; }

 varref = "$" n:name  { $$ = tlcall_from(TASK, tlACTIVE(tlSYM("_var_get")), tlACTIVE(n), null); }
 lookup = n:name      { $$ = tlACTIVE(n); }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tlINT(atoi(yytext)); }

  text = '"' '"'          { $$ = tlTextEmpty(); }
       | '"'  t:stext '"' { $$ = t }
       | '"' ts:ctext '"' { $$ = tlcall_from_list(TASK, tlACTIVE(tlSYM("text_cat")), L(ts)); }

 stext = < (!"$" !"\"" .)+ > { $$ = tlTextNewTake(TASK, unescape(yytext)); }
 ptext = "$("_ e:expr _")"   { $$ = e }
       | "$" l:lookup        { $$ = l }
 ctext = t:ptext ts:ctext    { $$ = tlListPrepend(TASK, ts, t); }
       | t:stext ts:ctext    { $$ = tlListPrepend(TASK, ts, t); }
       | t:ptext             { $$ = tlListNewFrom1(TASK, t); }
       | t:stext             { $$ = tlListNewFrom(TASK, t); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tlsym_from_copy(TASK, yytext); }

slcomment = "//" (!nl .)*
 icomment = "/*" (!"*/" .)* ("*/"|!.)
  comment = (slcomment nle | icomment)

     ssep = _ ";" _
  ssepend = _ (nl | "}" | ")" | "]" | slcomment nle | !.) __

      eos = _ (nl | ";" | slcomment nle) __
  eosfull = _ (nl | ";" | "}" | ")" | "]" | slcomment nle | !.) __
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

    if (!sa_object_send) sa_object_send = tlACTIVE(tlSYM("_object_send"));

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

