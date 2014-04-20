%{
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "tl.h"
#include "code.h"
#include "debug.h"

#include "platform.h"

#include "trace-off.h"

static tlHandle sa_send, sa_try_send, sa_this_send;
static tlSym s__undefined;
static tlSym s_send, s_try_send;
static tlSym s_String_cat, s_List_clone, s_Map_clone, s_Map_update, s_Map_inherit;
static tlSym s_Var_new, s_var_get, s_var_set;
static tlSym s_this, s_this_get, s_this_set, s_this_send;
static tlSym s_Task_new;
static tlSym s_and, s_or, s_xor, s_not;
static tlSym s_lt, s_lte, s_gt, s_gte, s_eq, s_neq, s_cmp;
static tlSym s_band, s_bor, s_bxor, s_bnot, s_lshift, s_rshift;
static tlSym s_add, s_sub, s_mul, s_div, s_idiv, s_mod, s_pow;
static tlSym s_main;
static tlSym s_get, s_set, s_slice, s__set;
static tlSym s_assert;
static tlSym s_string;
static tlSym s_block, s__match, s__nomatch;

typedef struct ParseContext {
    tlTask* task;
    tlString* str;
    tlString* file;
    int line;
    int at;
    int current_indent;
    int indents[100];
} ParseContext;

static inline int writesome(ParseContext* cx, char* buf, int len) {
    int towrite = tlStringSize(cx->str) - cx->at;
    if (towrite <= 0) return 0;
    if (towrite < len) len = towrite;
    memcpy(buf, tlStringData(cx->str) + cx->at, len);
    cx->at += len;
    return len;
}

// remove underscores from c string
static inline char* rmu(char* c) {
    char* r = c;
    char* res = c;
    while (true) {
        if (*c == '_') { c++; continue; }
        if (*c == 0) { *r = 0; break; }
        *r = *c; c++; r++;
    }
    return res;
}

bool push_indent(void* data);
bool pop_indent(void* data);
bool check_indent(void* data);

#define YYSTYPE tlHandle
#define YY_XTYPE ParseContext*
#define YY_INPUT(buf, len, max, cx) { len = writesome(cx, buf, max); }

void try_name(tlSym name, tlHandle v, ParseContext* cx) {
    if (tlActiveIs(v)) v = tl_value(v);
    if (tlCodeIs(v)) {
        tlCodeSetInfo_(tlCodeAs(v), cx->file, tlINT(cx->line), name);
    }
    assert(tlSymIs(name));
}

tlHandle map_activate(tlMap* map) {
    int i = 0;
    int argc = 0;
    for (int i = 0;; i++) {
        tlHandle v = tlMapValueIter(map, i);
        if (!v) break;
        if (tlActiveIs(v) || tlCallIs(v)) argc++;
    }
    if (!argc) return map;
    tlCall* call = tlCallNew(argc + 1, null);
    tlCallSetFn_(call, tl_active(s_Map_clone));
    tlCallSet_(call, 0, map);
    argc = 1;
    for (int i = 0;; i++) {
        tlHandle v = tlMapValueIter(map, i);
        if (!v) break;
        if (tlActiveIs(v) || tlCallIs(v)) {
            tlMapValueIterSet_(map, i, null);
            tlCallSet_(call, argc++, v);
        }
    }
    return call;
}
tlHandle list_activate(tlList* list) {
    int size = tlListSize(list);
    int argc = 0;
    for (int i = 0; i < size; i++) {
        tlHandle v = tlListGet(list, i);
        if (tlActiveIs(v) || tlCallIs(v)) argc++;
    }
    if (!argc) return list;
    tlCall* call = tlCallNew(argc + 1, null);
    tlCallSetFn_(call, tl_active(s_List_clone));
    tlCallSet_(call, 0, list);
    argc = 1;
    for (int i = 0; i < size; i++) {
        tlHandle v = tlListGet(list, i);
        if (tlActiveIs(v) || tlCallIs(v)) {
            tlListSet_(list, i, null);
            tlCallSet_(call, argc++, v);
        }
    }
    return call;
}

tlHandle call_activate(tlHandle in) {
    if (tlCallIs(in)) {
        tlCall* call = tlCallAs(in);
        for (int i = 0;; i++) {
            tlHandle v = tlCallValueIter(call, i);
            if (!v) break;
            tlHandle v2 = call_activate(v);
            if (v != v2) tlCallValueIterSet_(call, i, v2);
        }
        return tl_active(call);
    }
    return in;
}
tlHandle set_target(tlHandle on, tlHandle target) {
    if (!on) return target;
    if (tlCallIs(on)) {
        tlCall* call = tlCallAs(on);
        tlHandle fn = tlCallGetFn(call);

        if (fn == sa_send || fn == sa_try_send) {
            tlHandle oop = tlCallGet(call, 0);
            if (!oop) {
                tlCallSet_(call, 0, target);
                if (tlCallGet(call, 1) == s__set || tlCallGet(call, 1) == s_set) {
                    // try to find a.b.f += 1 (becomes a.b.f = a.b.f + 1)
                    // it works because we *mutate* the calls a.b sequence is the same call used twice
                    //$$ = tlCallSendFromList(sa_send, null, n, tlListEmpty());
                    //$$ = tlCallFrom(tl_active(s_add), $$, e, null);
                    //$$ = tlCallSendFromList(sa_send, null, s__set, tlListFrom(tlNull, n, tlNull, $$, null));
                    tlCall* sub = tlCallCast(tlCallGet(call, 3));
                    if (sub) {
                        tlCall* sub2 = tlCallCast(tlCallGet(sub, 0));
                        if (sub2 && tlCallGetFn(sub2) == sa_send && tlCallGet(sub2, 0) == null) {
                            trace("installing second target for +=");
                            tlCallSet_(sub2, 0, target);
                            return on;
                        }
                    }
                }
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
                case '"': t[j] = '"'; break;
                case '$': t[j] = '$'; break;
                default: j--; break;
            }
        } else {
            t[j] = s[i];
        }
    }
    t[j] = 0;
    return t;
}

#define FILE yyxvar->file
#define LINE tlINT(yyxvar->line)
#define L(l) tlListAs(l)

//#define YY_DEBUG
#define YY_STACK_SIZE 1024

%}

 start = hashbang b:body __ !.   { $$ = b; try_name(s_main, b, yyxvar); }
hashbang = __ "#"_"!" (!nl .)* nle __
         | __

  body = __ &{push_indent(G)} (l:line ts:stmsnl { $$ = tlCodeFrom(ts, FILE, l) }
                              |l:line           { $$ = tlCodeFrom(tlListEmpty(), FILE, l) }
                              ) &{pop_indent(G)}

stmsnl = __ &{check_indent(G)} t:stmssl (
            __ &{check_indent(G)} t2:stmssl { t = tlListCat(L(t), L(t2)) }
       )* { $$ = t }

stmssl = t:stm (
            ssep t2:stm { t = tlListCat(L(t), L(t2)) }
          | ssep
       )* { $$ = t }

   stm = varassign | singleassign | multiassign | noassign

 empty = "(" _ empty _ ")"
       | _

 guard = __ &{check_indent(G)} "{"_ empty _"}:" _ b:body {
           tlCodeSetIsBlock_(b, true);
           tlList* as = tlListFrom(tlNull, tlTrue, s_block, tl_active(b), null);
           $$ = call_activate(tlCallFromList(tl_active(s__match), as));
       }
       | __ &{check_indent(G)} "{" _ e:expr _ "}:" _ b:body {
           tlCodeSetIsBlock_(b, true);
           tlList* as = tlListFrom(tlNull, e, s_block, tl_active(b), null);
           $$ = call_activate(tlCallFromList(tl_active(s__match), as));
       }
guards = gs:guard { gs = tlListFrom1(gs); } (g:guard { gs = tlListAppend(gs, g); })* { $$ = gs; }
 gexpr = _ &{push_indent(G)} gs:guards l:line &{pop_indent(G)} {
           gs = tlListAppend(gs, call_activate(tlCallFromList(tl_active(s__nomatch), tlListEmpty())));
           $$ = tl_active(tlCodeFrom(gs, FILE, l));
           $$ = call_activate(tlCallFromList($$, tlListEmpty()));
       }
       | &{!pop_indent(G)}


anames =     n:name _","_ as:anames { $$ = tlListPrepend(L(as), n); }
       #| "*" n:name                 { $$ = tlListFrom1(n); }
       |     n:name                 { $$ = tlListFrom1(n); }

   varassign = "var"__"$" n:name __"="__ e:bpexpr {
                 $$ = tlListFrom(call_activate(
                             tlCallFrom(tl_active(s_Var_new), e, null)
                 ), n, null);
             }
             | "$" n:name _"="__ e:bpexpr {
                 $$ = tlListFrom1(call_activate(
                             tlCallFrom(tl_active(s_var_set), tl_active(n), e, null)
                 ));
             }
             | "@" n:name _"="__ e:bpexpr {
                 $$ = tlListFrom1(call_activate(
                             tlCallFrom(tl_active(s_this_set), tl_active(s_this), n, e, null)
                 ));
             }
             | "$" n:name _ o:op "="__ e:bpexpr {
                 $$ = tlCallFrom(tl_active(s_var_get), tl_active(n), null);
                 $$ = tlCallFrom(tl_active(o), $$, e, null);
                 $$ = tlListFrom1(call_activate(
                             tlCallFrom(tl_active(s_var_set), tl_active(n), $$, null)
                 ));
             }

singleassign = n:name    _"="__ e:fn    { $$ = tlListFrom(tl_active(e), n, null); try_name(n, e, yyxvar); }
             | n:name    _"="__ e:bpexpr { $$ = tlListFrom(e, n, null); try_name(n, e, yyxvar); }
 multiassign = ns:anames _"="__ e:bpexpr { $$ = tlListFrom(e, tlCollectFromList_(L(ns)), null); }
    noassign = e:selfapply  { $$ = tlListFrom1(e); }
             | e:bpexpr     { $$ = tlListFrom1(e); }


 block = fn
       | "("__ b:body __ ")" {
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }
       | __ b:body {
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }

    fn = "(" __ as:fargs __"->"__ b:body __ ")" {
           tlCodeSetArgs_(tlCodeAs(b), L(as));
           $$ = b;
       }
       | "(" __ as:fargs __"=>"__ b:body __ ")" {
           tlCodeSetArgs_(tlCodeAs(b), L(as));
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }
       | as:fargs _"->"_ b:body {
           tlCodeSetArgs_(tlCodeAs(b), L(as));
           $$ = b;
       }
       | as:fargs _"=>"_ b:body {
           tlCodeSetArgs_(tlCodeAs(b), L(as));
           tlCodeSetIsBlock_(b, true);
           $$ = b;
       }

fargs = a:farg __","__ as:fargs { $$ = tlListPrepend(L(as), a); }
      | a:farg                  { $$ = tlListFrom1(a); }
      |                         { $$ = tlListEmpty(); }

farg = "&&" n:name { $$ = tlListFrom2(n, tlCollectLazy); }
     | "**" n:name { $$ = tlListFrom2(n, tlCollectEager); }
     | "&" n:name { $$ = tlListFrom2(n, tlThunkNull); }
     |     n:name { $$ = tlListFrom2(n, tlNull); }


  expr = "!" b:block {
            $$ = tlCallFromList(tl_active(s_Task_new), tlListFrom2(tlNull, tl_active(b)));
       }
       | op_log

selfapply = "return" &eosfull {
            $$ = call_activate(tlCallFromList(tl_active(tlSYM("return")), tlListEmpty()));
          }
          | "break" &eosfull {
            $$ = call_activate(tlCallFromList(tl_active(tlSYM("break")), tlListEmpty()));
          }
          | "continue" &eosfull {
            $$ = call_activate(tlCallFromList(tl_active(tlSYM("continue")), tlListEmpty()));
          }

andor = !"[" !"(" _ !("and" !namechar) !("or" !namechar) _
not = ("not" !namechar)

bpexpr = gexpr
       | v:value andor as:pcargs _":"_ b:block {
           trace("primary args");
           as = tlCallFromList(null, as);
           $$ = call_activate(tlCallAddBlock(set_target(as, v), tl_active(b)));
       }
       | v:value t:ptail _":"_ b:block {
           $$ = call_activate(tlCallAddBlock(set_target(t, v), tl_active(b)));
       }
       | e:expr _":"_ b:block {
           $$ = call_activate(tlCallAddBlock(e, tl_active(b)));
       }
       | pexpr

 pexpr = "assert" andor < as:pcargs > &peosfull {
            as = tlListAppend2(L(as), s_string, tlStringFromCopy(yytext, 0));
            $$ = call_activate(tlCallFromList(tl_active(s_assert), as));
       }
       | "!" b:block {
            $$ = call_activate(tlCallFromList(tl_active(s_Task_new), tlListFrom2(tlNull, tl_active(b))));
       }
       | "@" n:name _"("__ as:cargs __")" t:ptail &peosfull {
           trace("this method + args()");
           $$ = set_target(t, tlCallSendFromList(sa_this_send, tl_active(s_this), n, as));
           $$ = call_activate($$);
       }
       | gexpr
       | !not v:value andor as:pcargs &peosfull {
           trace("primary args");
           as = tlCallFromList(null, as);
           $$ = call_activate(set_target(as, v));
       }
       | v:value t:ptail &peosfull { $$ = call_activate(set_target(t, v)); }
       | e:expr                    { $$ = call_activate(e); }

   met = "." { $$ = sa_send; }
       | "?" { $$ = sa_try_send; }
    op = "+" { $$ = s_add; }
       | "-" { $$ = s_sub; }
       | "*" { $$ = s_mul; }
       | "/" { $$ = s_div; }
       | "/|" { $$ = s_idiv; }
       | "%" { $$ = s_mod; }
       | "^" { $$ = s_pow; }
slicea = expr
       | _   { $$ = tlNull }

 ptail = _ m:met _ n:name _"("__ as:cargs __")" t:ptail {
           trace("primary method + args()");
           $$ = set_target(t, tlCallSendFromList(m, null, n, as));
       }
       | _ "." _ n:name _ o:op "=" __ e:pexpr {
           trace("set field op");
           $$ = tlCallSendFromList(sa_send, null, n, tlListEmpty());
           $$ = tlCallFrom(tl_active(o), $$, e, null);
           $$ = tlCallSendFromList(sa_send, null, s__set, tlListFrom(tlNull, n, tlNull, $$, null));
       }
       | _ "." _ n:name _"=" __ e:pexpr {
           trace("set field");
           $$ = tlCallSendFromList(sa_send, null, s__set, tlListFrom(tlNull, n, tlNull, e, null));
       }
       | _ m:met _ n:name andor as:pcargs {
           trace("primary method + args");
           $$ = tlCallSendFromList(m, null, n, as);
       }
       | _ m:met _ n:name t:ptail {
           trace("primary method");
           $$ = set_target(t, tlCallSendFromList(m, null, n, tlListEmpty()));
       }
       | _ "[" __ i:expr __"]"_ o:op "=" __ e:pexpr {
           trace("set array op");
           $$ = tlCallSendFromList(sa_send, null, s_get, tlListFrom2(tlNull, i));
           $$ = tlCallFrom(tl_active(o), $$, e, null);
           $$ = tlCallSendFromList(sa_send, null, s_set, tlListFrom(tlNull, i, tlNull, $$, null));
       }
       | _ "["__ i:expr __"]" _ "=" __ e:pexpr {
           trace("set array");
           $$ = tlCallSendFromList(sa_send, null, s_set, tlListFrom(tlNull, i, tlNull, e, null));
       }
       | _ "["__ i:expr __"]" t:ptail {
           trace("primary array get");
           $$ = set_target(t, tlCallSendFromList(sa_send, null, s_get, tlListFrom2(tlNull, i)));
       }
       | _ "["__ i1:slicea __":"__ i2:slicea __"]" t:ptail {
           trace("primary array slice");
           $$ = set_target(t, tlCallSendFromList(sa_send, null, s_slice, tlListFrom(tlNull, i1, tlNull, i2, null)));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

  tail = _"("__ as:cargs __")" t:tail {
           trace("function args");
           $$ = set_target(t, tlCallFromList(null, as));
       }
       | _ m:met _ n:name _"("__ as:cargs __")" t:tail {
           trace("method args()");
           $$ = set_target(t, tlCallSendFromList(m, null, n, as));
       }
       | _ m:met _ n:name t:tail {
           trace("method");
           $$ = set_target(t, tlCallSendFromList(m, null, n, tlListEmpty()));
       }
       | _ "["__ e:expr __"]" t:tail {
           trace("array get");
           $$ = set_target(t, tlCallSendFromList(sa_send, null, s_get, tlListFrom2(tlNull, e)));
       }
       | _ "["__ a1:slicea __":"__ a2:slicea __"]" t:tail {
           trace("array slice");
           $$ = set_target(t, tlCallSendFromList(sa_send, null, s_slice, tlListFrom(tlNull, a1, tlNull, a2, null)));
       }
       | _ {
           trace("no tail");
           $$ = null;
       }

# // without carg1, 1000 + x parses as 1000(x=true)
pcargs = l:carg1 __","__
                (r:carg __","__  { l = tlListCat(L(l), r); }
                )*
                r:carg          { l = tlListCat(L(l), r); }
                                 { $$ = l; }
       | r:pcarg                 { $$ = r; }

 cargs = f:fn                   { $$ = tlListFrom2(tlNull, tl_active(f)); }
#tlListFrom2(tlNull, f); }
       | l:carg
                (__","__ r:carg { l = tlListCat(L(l), r); }
                )*              { $$ = l; }
       |                        { $$ = tlListEmpty(); }

 pcarg = n:name _"="__ v:expr   { $$ = tlListFrom2(n, v); }
       | v:pexpr                { $$ = tlListFrom2(tlNull, v); }
 carg1 = n:name _"="__ v:expr   { $$ = tlListFrom2(n, v); }
       | v:expr                 { $$ = tlListFrom2(tlNull, v); }

  carg = "+"_ n:name            { $$ = tlListFrom2(n, tlTrue); }
       | "-"_ n:name            { $$ = tlListFrom2(n, tlFalse); }
       | n:name _"="__ v:expr   { $$ = tlListFrom2(n, v); }
       | v:expr                 { $$ = tlListFrom2(tlNull, v); }


op_log = l:op_not _ ("or"  __ r:op_not { l = tlCallFrom(tl_active(s_or), l, r, null); }
                  _ |"and" __ r:op_not { l = tlCallFrom(tl_active(s_and), l, r, null); }
                  _ |"xor" __ r:op_not { l = tlCallFrom(tl_active(s_xor), l, r, null); }
                    )*                 { $$ = l }
op_not = "not" __ r:op_cmp             { $$ = tlCallFrom(tl_active(s_not), r, null); }
       | op_cmp
op_cmp = l:op_add _ ("<=" __ r:op_add { l = tlCallFrom(tl_active(s_lte), l, r, null); }
                    |"<"  __ r:op_add { l = tlCallFrom(tl_active(s_lt), l, r, null); }
                    |">"  __ r:op_add { l = tlCallFrom(tl_active(s_gt), l, r, null); }
                    |">=" __ r:op_add { l = tlCallFrom(tl_active(s_gte), l, r, null); }
                    |"==" __ r:op_add { l = tlCallFrom(tl_active(s_eq), l, r, null); }
                    |"!=" __ r:op_add { l = tlCallFrom(tl_active(s_neq), l, r, null); }
                    )*                { $$ = l; }
op_add = l:op_mul _ ("+"  __ r:op_mul { l = tlCallFrom(tl_active(s_add), l, r, null); }
                    |"-"  __ r:op_mul { l = tlCallFrom(tl_active(s_sub), l, r, null); }
                    )*                { $$ = l; }
op_mul = l:op_bit _ ("*"  __ r:op_bit { l = tlCallFrom(tl_active(s_mul), l, r, null); }
                    |"/"  __ r:op_bit { l = tlCallFrom(tl_active(s_div), l, r, null); }
                    |"/|" __ r:op_bit { l = tlCallFrom(tl_active(s_idiv), l, r, null); }
                    |"%"  __ r:op_bit { l = tlCallFrom(tl_active(s_mod), l, r, null); }
                    )*                { $$ = l; }
op_bit = l:op_sht _ ("&"  __ r:op_sht { l = tlCallFrom(tl_active(s_band), l, r, null); }
                    |"|"  __ r:op_sht { l = tlCallFrom(tl_active(s_bor), l, r, null); }
                    )*                { $$ = l; }
op_sht = l:op_pow _ ("<<" __ r:op_pow { l = tlCallFrom(tl_active(s_lshift), l, r, null); }
                    |">>" __ r:op_pow { l = tlCallFrom(tl_active(s_rshift), l, r, null); }
                    )*                { $$ = l; }
op_pow = l:paren  _ ("^"  __ r:paren  { l = tlCallFrom(tl_active(s_pow), l, r, null); }
                    )*                { $$ = l; }

 paren = "assert"_"("__ < as:cargs > __")" t:tail {
            as = tlListPrepend2(L(as), s_string, tlStringFromCopy(yytext, 0));
            $$ = set_target(t, tlCallFromList(tl_active(s_assert), as));
       }
       | f:fn t:tail                { $$ = set_target(t, tl_active(f)); }
       | "("__ f:fn __")" t:tail    { $$ = set_target(t, tl_active(f)); }
       | "("__ e:pexpr __")" t:tail { $$ = set_target(t, e); }
       | "("__ b:body  __")" t:tail {
           tlCodeSetIsBlock_(b, true);
           $$ = set_target(t, tlCallFromList(tl_active(b), tlListEmpty()));
       }
       | v:value t:tail             { $$ = set_target(t, v); }


object = "{"__ is:items __"}"!":"  { $$ = map_activate(tlMapToObject_(tlMapFromPairs(L(is)))); }
       | "{"__"}"!":"              { $$ = map_activate(tlMapToObject_(tlMapEmpty())); }
   map = "["__ is:items __"]"  { $$ = map_activate(tlMapFromPairs(L(is))); }
       | "["__":"__"]"         { $$ = map_activate(tlMapEmpty()); }
 items = "{"__ ts:tms __"}"!":" eom is:items
                               { $$ = tlListCat(L(ts), L(is)); }
       | "{"__ ts:tms __"}"!":"    { $$ = L(ts); }
       | i:item eom is:items   { $$ = tlListPrepend(L(is), i); }
       | i:item                { $$ = tlListFrom1(i) }
  item = n:name _"="__ v:expr  { $$ = tlListFrom2(n, v); try_name(n, v, yyxvar); }
   tms = i:tm eom is:tms       { $$ = tlListPrepend(L(is), i); }
       | i:tm                  { $$ = tlListFrom1(i) }
    tm = n:name                { $$ = tlListFrom2(n, tl_active(n)); }

  list = "["__ is:litems __"]" { $$ = list_activate(L(is)); }
       | "["__"]"              { $$ = list_activate(tlListEmpty()); }
litems = v:expr eom is:litems  { $$ = tlListPrepend(L(is), v); }
       | v:expr                { $$ = tlListFrom1(v); }

 value = lit | number | chr | text | object | map | list | sym | varref | thisref | lookup

   lit = "true" !namechar      { $$ = tlTrue; }
       | "false" !namechar     { $$ = tlFalse; }
       | "null" !namechar      { $$ = tlNull; }
       | "undefined" !namechar { $$ = tlCallFrom(tl_active(s__undefined), null); }

 varref = "$" n:name  { $$ = tlCallFrom(tl_active(s_var_get), tl_active(n), null); }
thisref = "@" n:name  { $$ = tlCallFrom(tl_active(s_this_get), tl_active(s_this), n, null); }
 lookup = n:name      { $$ = tl_active(n); }

   sym = "#" n:name                 { $$ = n }
number = "0x" < [_0-9A-F]+ >        { $$ = tlPARSENUM(rmu(yytext), 16); }
       | "0b" < [_0-1]+ >           { $$ = tlPARSENUM(rmu(yytext), 2); }
       | < "-"? [0-9] [_0-9]* "." [0-9] [_0-9]* > { $$ = tlFLOAT(atof(rmu(yytext))); }
       | < "-"?               "." [0-9] [_0-9]* > { $$ = tlFLOAT(atof(rmu(yytext))); }
       | < "-"? [0-9] [_0-9]* "." !name >         { $$ = tlFLOAT(atof(rmu(yytext))); }
       | < "-"? [0-9] [_0-9]* >                   { $$ = tlPARSENUM(rmu(yytext), 10); }

   chr = "'\\r'"          { $$ = tlINT('\r'); }
       | "'\\n'"          { $$ = tlINT('\n'); }
       | "'\\t'"          { $$ = tlINT('\t'); }
       | '\'' < . > '\''  { $$ = tlINT(yytext[0]); }

  text = '"' '"'          { $$ = tlStringEmpty(); }
       | '"'  t:stext '"' { $$ = t }
       | '"' ts:ctext '"' { $$ = tlCallFromList(tl_active(s_String_cat), L(ts)); }

 stext = < ("\\$" | "\\\"" | "\\\\" | !"$" !"\"" .)+ > { $$ = tlStringFromTake(unescape(yytext), 0); }
 ptext = "$("_ e:paren _")"  { $$ = e }
       | "$("_ b:body  _")"  { tlCodeSetIsBlock_(b, true); $$ = tlCallFrom(tl_active(b), null); }
       | "$" l:lookup        { $$ = l }
 ctext = t:ptext ts:ctext    { $$ = tlListPrepend2(ts, tlNull, t); }
       | t:stext ts:ctext    { $$ = tlListPrepend2(ts, tlNull, t); }
       | t:ptext             { $$ = tlListFrom2(tlNull, t); }
       | t:stext             { $$ = tlListFrom2(tlNull, t); }

  name = < [a-zA-Z_][a-zA-Z0-9_]* > { $$ = tlSymFromCopy(yytext, 0); }
namechar = [a-zA-Z_][a-zA-Z0-9_]

slcomment = "//" (!nl .)*
 icomment = "/*" (!"*/" (nl|.))* ("*/"|!.)
  comment = (slcomment nle | icomment)

     ssep = _ ";" _
  ssepend = _ (nl | "}" | ")" | "]" | slcomment nle | !.) __

    eosnl = _ (nl | slcomment nle) __
      eos = _ (nl | ";" | slcomment nle) __
  eosfull = _ (nl | ";" | "}" | ")" | "]" | slcomment nle | !.) __
 peosfull = _ (nl | ":" | ";" | "}" | ")" | "]" | slcomment nle | !.) __
      eom = _ (nl | "," | slcomment nle) __
       nl = ("\n" | "\r\n" | "\r") { yyxvar->line += 1; }
      nle = nl | !.
       sp = [ \t]
        _ = (sp | icomment)*
       __ = (sp | nl | comment)*

    line = { $$ = tlINT(yyxvar->line); }

%%

int find_indent(GREG* G) {
    int indent = 1;
    while (true) {
        if (indent > G->pos) break;
        char c = G->buf[G->pos - indent];
        if (c == '\n' || c == '\r') break;
        indent++;
    }
    trace("FOUND INDENT: %d", indent);
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
    trace("PUSH NEW INDENT: %d", peek_indent(G));
    return true;
}
bool pop_indent(void* data) {
    GREG* G = (GREG*)data;
    G->data->current_indent--;
    assert(G->data->current_indent >= 0);
    trace("POP INDENT, CURRENT: %d", peek_indent(G));
    return true;
}
bool check_indent(void* data) {
    GREG* G = (GREG*)data;
    bool res = peek_indent(G) == find_indent(G);
    trace("CHECK INDENT: %s", res?"true":"false");
    return res;
}

tlHandle tlParse(tlString* str, tlString* file) {
    trace("\n----PARSING----\n%s----", tl_str(str));

    if (!s_send) {
        s__undefined = tlSYM("_undefined");
        s_send = tlSYM("_send");
        s_try_send = tlSYM("_try_send");
        s_this_send = tlSYM("_this_send");
        sa_send = tl_active(s_send);
        sa_try_send = tl_active(s_try_send);
        sa_this_send = tl_active(s_this_send);
        s_String_cat = tlSYM("_String_cat");
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
        s_band = tlSYM("band"); s_bor = tlSYM("bor"); s_bxor = tlSYM("bxor"); s_bnot = tlSYM("bnot");
        s_lshift = tlSYM("lsh"); s_rshift = tlSYM("rsh");
        s_eq = tlSYM("eq"); s_neq = tlSYM("neq"); s_cmp = tlSYM("cmp");
        s_add = tlSYM("add"); s_sub = tlSYM("sub");
        s_mul = tlSYM("mul"); s_div = tlSYM("div"); s_idiv = tlSYM("idiv");
        s_mod = tlSYM("mod"); s_pow = tlSYM("pow");
        s_main = tlSYM("main"); s_get = tlSYM("get"); s_set = tlSYM("set"); s_slice = tlSYM("slice"); s__set = tlSYM("_set");
        s_assert = tlSYM("assert"); s_string = tlSYM("string");
        s_block = tlSYM("block"); s__match = tlSYM("_match"); s__nomatch = tlSYM("_nomatch");
    }

    ParseContext data;
    data.file = file;
    data.at = 0;
    data.line = 1;
    data.str = str;
    data.current_indent = 0;
    data.indents[0] = 0;

    GREG g;
    yyinit(&g);
    g.data = &data;
    if (!yyparse(&g)) {
        warning("parser error around line %d", g.maxline + 1); // humans count from 1
        TL_THROW("parser error around line %d", g.maxline + 1); // humans count from 1
    }
    tlHandle v = g.ss;
    yydeinit(&g);

    trace("\n----PARSED----");
    //debugcode(tlCodeAs(v));
    return v;
}

