abstract = grammar ->

print "analyzing"

names = HashMap.new
grammar.each: rule -> names[rule.name] = rule

TERMS = [prim=true,token=true,indent=true,addindent=true,setindent=true,resetindent=true,anchor=true]

callersmap = HashMap.new
callingmap = HashMap.new
callers = rule, name ->
    assert name
    { TERMS[rule.type]    }: null
    { rule.type == "not"  }: callers(rule.term, name)
    { rule.type == "ahead"}: callers(rule.term, name)
    { rule.type == "name" }: callers(rule.term, name)
    { rule.type == "call" }: callersmap[rule.target] = (callersmap[rule.target] or 0) + 1
                             callingmap[name] = (callingmap[name] or 0) + 1
    { rule.type == "or"   }: rule.terms.each(t -> callers(t, name))
    { rule.type == "opt"  }: callers(rule.term, name)
    { rule.type == "star" }: callers(rule.term, name)
    { rule.type == "plus" }: callers(rule.term, name)
    { rule.type == "starsep" }: callers(rule.term, name); callers(rule.sep, name)
    { rule.type == "plussep" }: callers(rule.term, name); callers(rule.sep, name)
    { rule.type == "and"  }: rule.terms.each(t -> callers(t, name))
    {                     }: throw "unknown callers $(repr(rule))"
grammar.each: rule -> callers(rule, rule.name)

consumers = HashMap.new

// TODO fold this in with tracing??
// 2 == terminal
// 1 == maybe but might not consume tokens
// false == not terminal
isterm2 = rule, seen ->
    assert seen
    name = try(rule.name)
    seen = { not name        }: seen
           { seen.find(name) }: return false
           {                 }: seen.add(name)
    //print "seen:", try(rule.name), repr(seen)
    { rule.type == "prim" and try(rule.text) }: 2
    { TERMS[rule.type]    }: 1
    { rule.type == "call" }: isterm(names[rule.target], seen)
    { rule.type == "not"  }: if isterm(rule.term, seen): return 1
    { rule.type == "ahead"}: if isterm(rule.term, seen): return 1
    { rule.type == "name" }: isterm(rule.term, seen)
    { rule.type == "or"   }:
        var $type = 2
        rule.terms.each: term =>
            t = isterm(term, seen)
            if not t: return false
            if t == 1: $type = t
        return $type
    { rule.type == "opt"  }: if isterm(rule.term, seen): return 1
                             return false
    { rule.type == "star" }:
        if isterm(rule.term, seen): return 1
        return false
    { rule.type == "plus" }:
        isterm(rule.term, seen)
    { rule.type == "starsep" }:
        if isterm(rule.term, seen) and isterm(rule.sep, seen): return 1
        return false
    { rule.type == "plussep" }:
        if not isterm(rule.sep, seen): return false
        isterm(rule.term, seen)
    { rule.type == "and"  }:
        var $type = 1
        rule.terms.each: term =>
            t = isterm(term, seen)
            if not t: return false
            if t == 2: $type = t
        return $type
    {                     }: throw "unknown isterm $(repr(rule))"

isterm = rule, seen ->
    cache = consumers[rule]
    if isDefined(cache): return cache
    term = isterm2(rule, seen)
    consumers[rule] = term

// TODO cache as well, or merge with above??
directleft = HashMap.new
trace = rule ->
    step = rule, seen ->
        { rule.type == "prim" and try(rule.text) }: true
        { TERMS[rule.type]    }: false
        { rule.type == "call" }:
            if seen.find(rule.target):
                directleft[rule.target] = true
                return true, true
            step(names[rule.target], seen.add(rule.target))
        { rule.type == "not"  }: false
        { rule.type == "ahead"}: false
        { rule.type == "name" }: step(rule.term, seen)
        { rule.type == "opt"  }: false
        { rule.type == "star" }: false
        { rule.type == "starsep" }: false
        { rule.type == "plus" }: return step(rule.term, seen)
        { rule.type == "plussep" }: return step(rule.term, seen)
        { rule.type == "or"   }:
            rule.terms.each: term =>
                if not step(term, seen): return false
            return true
        { rule.type == "and"  }:
            rule.terms.each: term =>
                // handle type == "call" when it is direct left recursive
                consumes, direct = step(term, seen)
                if direct: continue
                if consumes: return true
            return false
        {                     }:
            throw "unknown step for: $(repr(rule))"
    step(rule, [rule.name])

grammar = grammar.map(rule ->
    terminal = isterm(rule, [])
    terminal = try(terminal >= 2) or false
    consumes = trace(rule)
    callers = callersmap[rule.name] or 0
    calling = callingmap[rule.name] or 0
    left = directleft[rule.name] or false
    print "rule:", rule.name, rule.type, "terminal:", terminal, "consumes:", consumes, "callers:", callers, "calling:", calling, "left:", left
    Object.set(rule, {{terminal, consumes, callers, calling, left}})
)
names.clear
grammar.each: rule -> names[rule.name] = rule

grammar.sort(l, r -> max(l.callers, l.calling) > max(r.callers, r.calling))[:40].each: rule, n ->
    print n, rule.name, "callers:", rule.callers, "calling:", rule.calling, "term:", rule.terminal, "left:", rule.left


print "direct left recursions:", directleft.size

lines = Array.new
lines.add "#include \"tlmeta.c\""
lines.add ""

emit_declare = rule ->
    lines.add "static State r_$(rule.name)(Parser*, int, int, bool);"

grammar.each(r -> emit_declare(r))
lines.add ""

can_optimize_and = t ->
    if not try(t.type == "and"): return false
    if try(t.act): return false
    if t.terms.size != 1: return false
    if t.terms[1].type == "name": return false
    true

directterm = term ->
    if term.type != "call": return
    if not directleft[term.target]: return
    rule = names[term.target]
    if rule.type != "and": return
    rule.terms.first

// returns true if this parse always consumes input if successful
consumes = term ->
    { term.type == "call" }: return names[term.target].consumes
    { term.type == "prim" }: return try(term.text) != null
    { term.type == "name" }: return consumes(term.term)
    { term.type == "token"}: return false
    { }: throw "unknown consumes term:"+ repr(term)

emit = term, ignored ->
    signored = if(ignored,"true","ignored")
    { term.type == "call" }: "r_$(term.target)(_p, _pos, _start == _pos? _rec : 0, $signored);"
    { term.type == "prim" and try(term.text) }: "prim_$(term.target)(_p, _pos, \"$(term.text.escape(true))\", $signored);"
    { term.type == "prim" }: "prim_$(term.target)(_p, _pos, $signored);"
    { term.type == "name" }: return emit(term.term, ignored)
    { term.type == "token"}: "state_ok(_pos, $signored?tlNull:tlSTR(\"$(term.token.escape(true))\"));"
    { }: throw "unknown emit term:"+ repr(term)

var $nextcache = 1
rectokens = HashMap.new
rectoken = name ->
    if not rectokens[name]: rectokens[name] = rectokens.size
    1 << rectokens[name]

emit_rule = rule ->
    lines.add "static State r_$(rule.name)(Parser* _p, const int _start, int _rec, bool ignored) { // $(rule.type)"
    cache = { rule.name == "expr"   }: $nextcache += 1
            { rule.name == "mexpr"  }: $nextcache += 1
            { rule.name == "value"  }: $nextcache += 1
            { rule.name == "body"   }: $nextcache += 1
            { rule.name == "bodynl" }: $nextcache += 1
            {                       }: 0
    if cache > 0:
        lines.add " State _c = cached(_p, \"$(rule.name)\", _start, $cache);"
        lines.add " if (_c.ok) return parser_pass(_p, \"$(rule.name)\", _start, _c, 0);"
    lines.add " parser_enter(_p, \"$(rule.name)\", _start);"
    lines.add " int _pos = _start;"
    { can_optimize_and(rule) }:
        assert not directleft[rule.name]
        lines.add " State _r = "+ emit(rule.terms[1])
        if consumes(rule.terms[1]):
            lines.add " if (_r.ok) parser_commit(_p, _r.pos, \"$(rule.name)\", 1);"
        lines.add " if (_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, _r, $cache);"
        lines.add " if (_p->error_line) { /*print(\"expect: $(rule.name)\");*/ return _r; }"
        lines.add " return parser_fail(_p, \"$(rule.name)\", _start);"
    { rule.type == "not" }:
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term, true)
        lines.add " if (_r.ok) return parser_fail(_p, \"$(rule.name)\", _start);"
        lines.add " return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_start, tlNull), $cache);"
    { rule.type == "ahead" }:
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term, true)
        lines.add " if (!_r.ok) return parser_fail(_p, \"$(rule.name)\", _start);"
        lines.add " return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_start, tlNull), $cache);"
    { rule.type == "opt" }:
        lines.add " tlHandle* _v = tlNull;"
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (_r.ok) { _v = _r.value; _pos = _r.pos; }"
        lines.add " return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, _v), $cache);"
    { rule.type == "star" }:
        lines.add " tlArray* arr = ignored? 0 : tlArrayNew();"
        lines.add " State _r;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add " goto step1;"
    { rule.type == "starsep" }:
        lines.add " tlArray* arr = ignored? 0 : tlArrayNew();"
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.sep, true)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " int _pos2 = _r.pos;"
        lines.add " _pos = _r.pos;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos2, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add " goto step1;"
    { rule.type == "plus" }:
        lines.add " tlArray* arr = ignored? 0 : tlArrayNew();"
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_fail(_p, \"$(rule.name)\", _start);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add " goto step1;"
    { rule.type == "plussep" }:
        lines.add " tlArray* arr = ignored? 0 : tlArrayNew();"
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_fail(_p, \"$(rule.name)\", _start);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.sep, true)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " int _pos2 = _r.pos;"
        lines.add " _pos = _r.pos;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos2, arr? tlArrayToList(arr) : tlNull), $cache);"
        lines.add " if (arr) tlArrayAdd(arr, _r.value);"
        lines.add " _pos = _r.pos;"
        lines.add " goto step1;"
    { rule.type == "or" }:
        directs = Array.new
        lines.add " State _r;"
        if rule.terms.map(t -> directterm(t)).filter.size > 0:
            rec = rectoken(rule.name)
            lines.add " if ((_rec & $rec) == $rec) return parser_fail(_p, \"$(rule.name)\", _start);"
            lines.add " _rec |= $rec;"
        rule.terms.each: term ->
            direct = directterm(term)
            { direct }: if directs.find(direct): continue
                        directs.add(direct)
                        lines.add " _r = "+ emit(direct)
                        lines.add " if (_r.ok) goto directs;"
            {        }: lines.add " _r = "+ emit(term)
                        if directs.size > 0:
                            lines.add " if (_r.ok) goto directs;"
                        if directs.size == 0:
                            lines.add " if (_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, _r, $cache);"
                            lines.add " if (_p->error_line) { /*print(\"expect: $(rule.name)\");*/ return _r; }"
        lines.add " return parser_fail(_p, \"$(rule.name)\", _start);"
        if directs.size > 0:
            lines.add "directs:;"
            lines.add " assert(_r.ok);"
            lines.add " State _r2 = _r;"
            lines.add " _pos = _r.pos;"
            rule.terms.each: term ->
                if not directterm(term): continue
                lines.add " _r ="+ emit(term)
                lines.add " if (_r.ok) goto directs;"
            lines.add " return parser_pass(_p, \"$(rule.name)\", _start, _r2, $cache);"
    { rule.type == "and" }:
        anchor = Var.new
        indent = Var.new
        names = Array.new
        lines.add " State _r;"
        rule.terms.each: term, n ->
            // skip first term if directleft recursive, or parent will take care of it
            if n == 1 and directleft[rule.name]: continue
            if term.type == "resetindent":
                lines.add " const int __indent = 0;"
                lines.add " const int _indent = _p->indent; _p->indent = __indent;"
                $indent = true
                return
            if term.type == "addindent":
                lines.add " const int __indent = parser_indent(_p, _pos);"
                lines.add " if (_p->indent >= __indent) return parser_fail(_p, \"$(rule.name)\", _pos);"
                lines.add " const int _indent = _p->indent; _p->indent = __indent;"
                $indent = true
                return
            if term.type == "setindent":
                lines.add " const int __indent = parser_indent(_p, _pos);"
                lines.add " if (_p->indent > __indent) return parser_fail(_p, \"$(rule.name)\", _pos);"
                lines.add " const int _indent = _p->indent; _p->indent = __indent;"
                $indent = true
                return
            if term.type == "indent":
                restore = { $indent }: "_p->indent = _indent; "
                          {         }: ""
                lines.add " if (_p->indent != parser_indent(_p, _pos)) { $(restore)return parser_fail(_p, \"$(rule.name)\", _pos); }"
                return
            if term.type == "anchor":
                { not $anchor }: lines.add " const char* _anchor = parser_set_anchor(_p, \"$(term.msg.escape(true))\");"
                {             }: lines.add " _anchor = parser_set_anchor(_p, \"$(term.msg.escape(true))\");"
                $anchor = true
                return
            ignored = { term.type == "name"  }: false
                      { n == rule.terms.size }: names.size > 0
                      {                      }: true
            lines.add " _r = "+ emit(term, ignored)
            restore = { $indent }: "_p->indent = _indent; "
                      {         }: ""
            if not $anchor:
                lines.add " if (!_r.ok) { $(restore)return parser_fail(_p, \"$(rule.name)\", _pos); }"
            if $anchor:
                lines.add " if (!_r.ok) { $(restore)return parser_error(_p, \"$(rule.name)\", _start, _pos); }"
            if term.type == "name":
                assert not ignored
                names.add term.value
                lines.add " tlHandle $(term.value) = _r.value;"
            lines.add " _pos = _r.pos;"
            if consumes(term):
                lines.add " parser_commit(_p, _pos, \"$(rule.name)\", $n);"
        { try(rule.act)  }:
            lines.add " tlHandle _v = tlNull; if (!ignored) { _v = $(rule.act); }"
        { names.size > 0 }:
            inner = names.toList.map(n -> "\"$n\", $n").join(", ")
            lines.add " tlHandle _v = tlObjectFrom($inner, null);"
        {                }:
            lines.add " tlHandle _v = _r.value;"
        if $anchor: lines.add " _p->anchor = _anchor;"
        if $indent: lines.add " _p->indent = _indent;"
        lines.add " return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, _v), $cache);"
    { }: throw "unknown emit_rule: " +repr(rule)
    lines.add "}"
grammar.each(r -> emit_rule(r))

return lines.join("\n")

