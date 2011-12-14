%{
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "tl.h"
#include "code.h"
#include "debug.h"

#include "platform.h"

#include "trace-off.h"

static tlValue sa_object_send, sa_this_send;
static tlSym s_object_send;
static tlSym s_Text_cat, s_List_clone, s_Map_clone, s_Map_update, s_Map_inherit;
static tlSym s_Var_new, s_var_get, s_var_set;
static tlSym s_this, s_this_get, s_this_set, s_this_send;
static tlSym s_Task_new;
static tlSym s_and, s_or, s_xor, s_not;
static tlSym s_lt, s_lte, s_gt, s_gte, s_eq, s_neq, s_cmp;
static tlSym s_add, s_sub, s_mul, s_div, s_mod, s_pow;
static tlSym s_main;
static tlSym s_get;
static tlSym s_assert;
static tlSym s_text;

typedef struct ParseContext {
    tlTask* task;
    tlText* text;
    tlText* file;
    int line;
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
    if (tlCodeIs(v)) {
        tlCodeSetName_(tlCodeAs(v), name);
    }
    assert(tlSymIs(name));
}

tlValue map_activate(tlMap* map) {
    int i = 0;
    int argc = 0;
    for (int i = 0;; i++) {
        tlValue v = tlMapValueIter(map, i);
        if (!v) break;
        if (tlActiveIs(v) || tlCallIs(v)) argc++;
    }
    if (!argc) return map;
    tlCall* call = tlCallNew(null, argc + 1, null);
    tlCallSetFn_(call, tl_active(s_Map_clone));
    tlCallSet_(call, 0, map);
    argc = 1;
    for (int i = 0;; i++) {
        tlValue v = tlMapValueIter(map, i);
        if (!v) break;
        if (tlActiveIs(v) || tlCallIs(v)) {
            tlMapValueIterSet_(map, i, null);
            tlCallSet_(call, argc++, v);
        }
    }
    return call;
}
tlValue list_activate(tlList* list) {
    int size = tlListSize(list);
    int argc = 0;
    for (int i = 0; i < size; i++) {
        tlValue v = tlListGet(list, i);
        if (tlActiveIs(v) || tlCallIs(v)) argc++;
    }
    if (!argc) return list;
    tlCall* call = tlCallNew(null, argc + 1, null);
    tlCallSetFn_(call, tl_active(s_List_clone));
    tlCallSet_(call, 0, list);
    argc = 1;
    for (int i = 0; i < size; i++) {
        tlValue v = tlListGet(list, i);
        if (tlActiveIs(v) || tlCallIs(v)) {
            tlListSet_(list, i, null);
            tlCallSet_(call, argc++, v);
        }
    }
    return call;
}

tlValue call_activate(tlValue in) {
    if (tlCallIs(in)) {
        tlCall* call = tlCallAs(in);
        for (int i = 0;; i++) {
            tlValue v = tlCallValueIter(call, i);
            if (!v) break;
            tlValue v2 = call_activate(v);
            if (v != v2) tlCallValueIterSet_(call, i, v2);
        }
        return tl_active(call);
    }
    return in;
}
tlValue set_target(tlValue on, tlValue target) {
    if (!on) return target;
    if (tlCallIs(on)) {
        tlCall* call = tlCallAs(on);
        tlValue fn = tlCallGetFn(call);

        if (fn == sa_object_send) {
            tlValue oop = tlCallGet(call, 0);
            if (!oop) {
                tlCallSet_(call, 0, target);
                return on;
            } else {
                set_target(oop, target);
                return on;
            }
        }

        if (!fn) {
            tlCallSetFn_(call, target);
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

//#define YY_DEBUG
#define YY_STACK_SIZE 1024

%}

 start = hashbang b:body __ !.   { $$ = b; try_name(s_main, b); }
hashbang = __ "#"_"!" (!nl .)* nle __
         | __

  body = ts:stms           { $$ = tlCodeFrom(TASK, ts); }

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
                             tlCallFrom(TASK, tl_active(s_Var_new), e, null)
                 ), n, null);
             }
             | "$" n:name _"="__ e:bpexpr {
                 $$ = tlListFrom1(TASK, call_activate(
                             tlCallFrom(TASK, tl_active(s_var_set), tl_active(n), e, null)
                 ));
             }
             | "@" n:name _"="__ e:bpexpr {
                 $$ = tlListFrom1(TASK, call_activate(
                             tlCallFrom(TASK, tl_active(s_this_set), tl_active(s_this), n, e, null)
                 ));
             }
#// TODO for now just add and subtract, until we do full operators
             | "$" n:name _"+"_"="__ e:bpexpr {
                 $$ = tlCallFrom(TASK, tl_active(s_var_get), tl_active(n), null);
                 $$ = tlCallFrom(TASK, tl_active(s_add), $$, e, null);
                 $$ = tlListFrom1(TASK, call_activate(
                             tlCallFrom(TASK, tl_active(s_var_set), tl_active(n), $$, null)
                 ));
             }
             | "$" n:name _"-"_"="__ e:bpexpr {
                 $$ = tlCallFrom(TASK, tl_active(s_var_get), tl_active(n), null);
                 $$ = tlCallFrom(TASK, tl_active(s_sub), $$, e, null);
                 $$ = tlListFrom1(TASK, call_activate(
                             tlCallFrom(TASK, tl_active(s_var_set), tl_active(n), $$, null)
                 ));
             }

singleassign = n:name    _"="__ e:fn    { $$ = tlListFrom(TASK, tl_active(e), n, null); try_name(n, e); }
             | n:name    _"="__ e:bpexpr { $$ = tlListFrom(TASK, e, n, null); try_name(n, e); }
 multiassign = ns:anames _"="__ e:bpexpr { $$ = tlListFrom(TASK, e, tlCollectFromList_(L(ns)), null); }
    noassign = e:selfapply  { $$ = tlListFrom1(TASK, e); }
             | e:bpexpr     { $$ = tlListFrom1(TASK, e); }


 block = b:fn {
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }
       | "("__ b:body __ ")" {
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }
       | ts:stmsnl &ssepend {
           b = tlCodeFrom(TASK, ts);
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }

    fn = "(" __ as:fargs __"->"__ b:body __ ")" {
           tlCodeSetArgs_(TASK, tlCodeAs(b), L(as));
           $$ = b;
       }
       | as:fargs _"->"_ ts:stmsnl &ssepend {
           b = tlCodeFrom(TASK, ts);
           tlCodeSetArgs_(TASK, tlCodeAs(b), L(as));
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
            $$ = tlCallFromList(TASK,
                        tl_active(s_Task_new), tlListFrom2(TASK, tlNull, tl_active(b)));
       }
       | op_log

selfapply = n:name _ &eosfull {
    $$ = call_activate(tlCallFromList(TASK, tl_active(n), tlListEmpty()));
}

bpexpr = v:value t:ptail _":"_ b:block {
           $$ = call_activate(tlCallAddBlock(TASK, set_target(t, v), tl_active(b)));
       }
       | e:expr _":"_ b:block {
           $$ = call_activate(tlCallAddBlock(TASK, e, tl_active(b)));
       }
       | pexpr

 pexpr = "assert" _!"(" < as:pcargs > &peosfull {
            as = tlListAppend2(TASK, L(as), s_text, tlTextFromCopy(TASK, yytext, 0));
            $$ = call_activate(tlCallFromList(TASK, tl_active(s_assert), as));
       }
       | "!" b:block {
            $$ = call_activate(tlCallFromList(TASK,
                        tl_active(s_Task_new), tlListFrom2(TASK, tlNull, tl_active(b))));
       }
       | "@" n:name _"("__ as:cargs __")" t:ptail &peosfull {
           trace("this method + args()");
           $$ = set_target(t, tlCallSendFromList(TASK, sa_this_send, tl_active(s_this), n, as));
           $$ = call_activate($$);
       }
       | v:value t:ptail &peosfull { $$ = call_activate(set_target(t, v)); }
       | e:expr                    { $$ = call_activate(e); }

# // TODO add [] and oop.method: and oop.method arg1: ... etc
 ptail = _!"(" as:pcargs {
           trace("primary args");
           $$ = tlCallFromList(TASK, null, as);
       }
       | _"."_ n:name _"("__ as:cargs __")" t:ptail {
           trace("primary method + args()");
           $$ = set_target(t, tlCallSendFromList(TASK, sa_object_send, null, n, as));
       }
       | _"."_ n:name _ as:pcargs {
           trace("primary method + args");
           $$ = tlCallSendFromList(TASK, sa_object_send, null, n, as);
       }
       | _"."_ n:name t:ptail {
           trace("primary method");
           $$ = set_target(t, tlCallSendFromList(TASK, sa_object_send, null, n, tlListEmpty()));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

  tail = _"("__ as:cargs __")" t:tail {
           trace("function args");
           $$ = set_target(t, tlCallFromList(TASK, null, as));
       }
       | _"."_ n:name _"("__ as:cargs __")" t:tail {
           trace("method args()");
           $$ = set_target(t, tlCallSendFromList(TASK, sa_object_send, null, n, as));
       }
       | _"."_ n:name t:tail {
           trace("method");
           $$ = set_target(t, tlCallSendFromList(TASK, sa_object_send, null, n, tlListEmpty()));
       }
       | _"["__ e:expr __"]" t:tail {
           trace("array get");
           $$ = set_target(t, tlCallSendFromList(TASK, sa_object_send, null, s_get, as));
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


op_log = l:op_not _ ("or"  __ r:op_not { l = tlCallFrom(TASK, tl_active(s_or), l, r, null); }
                  _ |"and" __ r:op_not { l = tlCallFrom(TASK, tl_active(s_and), l, r, null); }
                  _ |"xor" __ r:op_not { l = tlCallFrom(TASK, tl_active(s_xor), l, r, null); }
                    )*                 { $$ = l }
op_not = "not" __ r:op_cmp             { $$ = tlCallFrom(TASK, tl_active(s_not), r, null); }
       | op_cmp
op_cmp = l:op_add _ ("<=" __ r:op_add { l = tlCallFrom(TASK, tl_active(s_lte), l, r, null); }
                    |"<"  __ r:op_add { l = tlCallFrom(TASK, tl_active(s_lt), l, r, null); }
                    |">"  __ r:op_add { l = tlCallFrom(TASK, tl_active(s_gt), l, r, null); }
                    |">=" __ r:op_add { l = tlCallFrom(TASK, tl_active(s_gte), l, r, null); }
                    |"==" __ r:op_add { l = tlCallFrom(TASK, tl_active(s_eq), l, r, null); }
                    |"!=" __ r:op_add { l = tlCallFrom(TASK, tl_active(s_neq), l, r, null); }
                    )*                { $$ = l; }
op_add = l:op_mul _ ("+" __ r:op_mul { l = tlCallFrom(TASK, tl_active(s_add), l, r, null); }
                    |"-" __ r:op_mul { l = tlCallFrom(TASK, tl_active(s_sub), l, r, null); }
                    )*               { $$ = l; }
op_mul = l:op_pow _ ("*" __ r:op_pow { l = tlCallFrom(TASK, tl_active(s_mul), l, r, null); }
                    |"/" __ r:op_pow { l = tlCallFrom(TASK, tl_active(s_div), l, r, null); }
                    |"%" __ r:op_pow { l = tlCallFrom(TASK, tl_active(s_mod), l, r, null); }
                    )*               { $$ = l; }
op_pow = l:paren  _ ("^" __ r:paren  { l = tlCallFrom(TASK, tl_active(s_pow), l, r, null); }
                    )*

 paren = "assert"_"("__ < as:cargs > __")" t:tail {
            as = tlListPrepend2(TASK, L(as), s_text, tlTextFromCopy(TASK, yytext, 0));
            $$ = set_target(t, tlCallFromList(TASK, tl_active(s_assert), as));
       }
       | f:fn t:tail                { $$ = set_target(t, tl_active(f)); }
       | "("__ e:pexpr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body  __")" t:tail {
           tlCodeSetIsBlock_(b, true);
           $$ = set_target(t, tlCallFromList(TASK, tl_active(b), tlListEmpty()));
       }
       | v:value t:tail             { $$ = set_target(t, v); }


object = "{"__ is:items __"}"  { $$ = map_activate(tlMapToObject_(tlMapFromPairs(TASK, L(is)))); }
       | "{"__"}"              { $$ = map_activate(tlMapToObject_(tlMapFromPairs(TASK, L(is)))); }
   map = "["__ is:items __"]"  { $$ = map_activate(tlMapFromPairs(TASK, L(is))); }
       | "["__":"__"]"         { $$ = map_activate(tlMapEmpty()); }
 items = i:item eom is:items   { $$ = tlListPrepend(TASK, L(is), i); }
       | i:item                { $$ = tlListFrom1(TASK, i) }
  item = n:name _":"__ v:expr  { $$ = tlListFrom2(TASK, n, v); try_name(n, v); }

  list = "["__ is:litems __"]" { $$ = list_activate(L(is)); }
       | "["__"]"              { $$ = list_activate(tlListEmpty()); }
litems = v:expr eom is:litems  { $$ = tlListPrepend(TASK, L(is), v); }
       | v:expr                { $$ = tlListFrom1(TASK, v); }

 value = lit | number | text | object | map | list | sym | varref | thisref | lookup

   lit = "true"      { $$ = tlTrue; }
       | "false"     { $$ = tlFalse; }
       | "null"      { $$ = tlNull; }
       | "undefined" { $$ = tlUndefined; }

 varref = "$" n:name  { $$ = tlCallFrom(TASK, tl_active(s_var_get), tl_active(n), null); }
thisref = "@" n:name  { $$ = tlCallFrom(TASK, tl_active(s_this_get), tl_active(s_this), n, null); }
 lookup = n:name      { $$ = tl_active(n); }

   sym = "#" n:name                 { $$ = n }
number = < "-"? [0-9]+ >            { $$ = tlINT(atoi(yytext)); }

  text = '"' '"'          { $$ = tlTextEmpty(); }
       | '"'  t:stext '"' { $$ = t }
       | '"' ts:ctext '"' { $$ = tlCallFromList(TASK, tl_active(s_Text_cat), L(ts)); }

 stext = < (!"$" !"\"" .)+ > { $$ = tlTextFromTake(TASK, unescape(yytext), 0); }
 ptext = "$("_ e:paren _")"  { $$ = e }
       | "$("_ b:body  _")"  { tlCodeSetIsBlock_(b, true); $$ = tlCallFrom(TASK, tl_active(b), null); }
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
       nl = "\n" | "\r\n" | "\r" { yyxvar->line += 1; }
      nle = nl | !.
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

tlValue tlParse(tlTask* task, tlText* text, tlText* file) {
    trace("\n----PARSING----\n%s----", tl_str(text));

    if (!s_object_send) {
        s_object_send = tlSYM("_object_send");
        s_this_send = tlSYM("_this_send");
        sa_object_send = tl_active(s_object_send);
        sa_this_send = tl_active(s_this_send);
        s_Text_cat = tlSYM("_Text_cat");
        s_List_clone = tlSYM("_List_clone");
        s_Map_clone = tlSYM("_Map_clone");
        s_Map_update = tlSYM("_Map_udpate");
        s_Map_inherit = tlSYM("_Map_inherit");
        s_Var_new = tlSYM("_Var_new");
        s_var_get = tlSYM("_var_get");
        s_var_set = tlSYM("_var_set");
        s_this = tlSYM("this");
        s_this_get = tlSYM("_this_get");
        s_this_set = tlSYM("_this_set");
        s_Task_new = tlSYM("_Task_new");
        s_and = tlSYM("and"); s_or = tlSYM("or"), s_xor = tlSYM("xor"), s_not = tlSYM("not");
        s_lt = tlSYM("lt"); s_lte = tlSYM("lte"); s_gt = tlSYM("gt"); s_gte = tlSYM("gte");
        s_eq = tlSYM("eq"); s_neq = tlSYM("neq"); s_cmp = tlSYM("cmp");
        s_add = tlSYM("add"); s_sub = tlSYM("sub");
        s_mul = tlSYM("mul"); s_div = tlSYM("div");
        s_mod = tlSYM("mod"); s_pow = tlSYM("pow");
        s_main = tlSYM("main"); s_get = tlSYM("get");
        s_assert = tlSYM("assert"); s_text = tlSYM("text");
    }

    ParseContext data;
    data.file = file;
    data.at = 0;
    data.line = 0;
    data.text = text;
    data.current_indent = 0;
    data.indents[0] = 0;

    GREG g;
    yyinit(&g);
    g.data = &data;
    if (!yyparse(&g)) {
        TL_THROW("parser error: %d", g.pos + g.offset);
    }
    tlValue v = g.ss;
    yydeinit(&g);

    trace("\n----PARSED----");
    //debugcode(tlCodeAs(v));
    return v;
}

