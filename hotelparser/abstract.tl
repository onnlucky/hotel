grammar = io.File("calc.tld").readString.eval
names = HashMap.new
grammar.each: rule -> names[rule.name] = rule

isterm = rule, seen ->
    assert seen
    name = try(rule.name)
    seen = { not name        }: seen
           { seen.find(name) }: return false
           {                 }: seen.add(name)
    //print "seen:", try(rule.name), repr(seen)
    { rule.type == "prim" }: return true
    { rule.type == "call" }: return isterm(names[rule.target], seen)
    { rule.type == "or"   }:
        var $type = true
        rule.terms.each: term =>
            t = isterm(term, seen)
            if not t: return false
            if t == "maybe": $type = t
        return $type
    { rule.type == "star" }:
        if isterm(rule.term, seen): return "maybe"
        return false
    { rule.type == "plus" }:
        return isterm(rule.term, seen)
    { rule.type == "starsep" }:
        if isterm(rule.term, seen) and isterm(rule.sep, seen): return "maybe"
    { rule.type == "plussep" }:
        return isterm(rule.term, seen) and isterm(rule.sep, seen)
    { rule.type == "and" }:
        var $type = "maybe"
        rule.terms.each: term =>
            t = isterm(term, seen)
            if not t: return false
            if t == true: $type = t
        return $type
    {                    }: throw "unknown isterm $(repr(rule))"

callers = rule, name ->
    { rule.type == "prim" }: 0
    { rule.type == "call" }: if(rule.target==name, 1, 0)
    { rule.type == "or"   }: rule.terms.map(t -> callers(t, name)).sum
    { rule.type == "star" }: callers(rule.term, name)
    { rule.type == "plus" }: callers(rule.term, name)
    { rule.type == "starsep" }: callers(rule.term, name) + callers(rule.sep, name)
    { rule.type == "plussep" }: callers(rule.term, name) + callers(rule.sep, name)
    { rule.type == "and" }: rule.terms.map(t -> callers(t, name)).sum
    {                    }: throw "unknown callers $(repr(rule))"
allcallers = rule ->
    grammar.map(r -> callers(r, rule.name)).sum

grammar = grammar.map(rule ->
    terminal = isterm(rule, [])
    callers = allcallers(rule)
    Object.set(rule, {{terminal, callers}})
)
names.clear
grammar.each: rule -> names[rule.name] = rule


recursive = name, up, direct ->
    if not up: return
    if up.rule == name: return up.up, up.at, direct and up.at == 2
    recursive(name, up.up, direct and up.at == 2)

directleft = HashMap.new
recpoints = HashMap.new
terminals = HashMap.new

trace = up ->
    outs = Array.new

    upstep = up ->
        if not up: return
        step(names[up.rule], up.at, up.up)

    step = rule, at, up ->
        //print try(rule.name), at, repr(up)
        { rule.type == "prim" or try(rule.terminal) }:
            if not outs.find(up): outs.add(up)
            // collect all reasons why we find this terminal
            terms = terminals[rule] or []
            term = {rule=up.rule, at=up.at}
            if not terms.find(term): terminals[rule] = terms.add(term)
        { rule.type == "call" }: step(names[rule.target], at, up)
        { rule.type == "or"   }: rule.terms.each(term, n -> step(term, 1, up))
        { rule.type == "star" }:
            { at == 1 }: step(rule.term, 1, {rule=rule.name, at=1, up=up})
                         upstep(up)
        { rule.type == "plus" }:
            { at == 1 }: step(rule.term, 1, {rule=rule.name, at=2, up=up})
            { at == 2 }: step(rule.term, 1, {rule=rule.name, at=2, up=up})
                         upstep(up)
        { rule.type == "starsep" }:
            { at == 1 }: step(rule.term, 1, {rule=rule.name, at=2, up=up})
                         upstep(up)
            { at == 2 }: step(rule.sep, 1, {rule=rule.name, at=3, up=up})
                         upstep(up)
            { at == 3 }: step(rule.term, 1, {rule=rule.name, at=2, up=up})
        { rule.type == "and" }:
            rup, rat, direct = recursive(rule.name, up, true)
            if rat:
                points = recpoints[rule.name] or []
                point = {at=rat - 1, direct=direct}
                if not points.find(point): recpoints[rule.name] = points.add(point)
                { direct   }: directleft[rule.name] = true
                { rat > rule.terms.size }: null
                {          }: step(rule.terms[at], 1, {rule=rule.name, at=at + 1, up=rup})
                upstep(up)
                return
            if at > rule.terms.size:
                upstep(up)
                return
            step(rule.terms[at], 1, {rule=rule.name, at=at + 1, up=up})
        {                    }:
            throw "unknown step for: $(repr(rule))"

    //print "!!tracing:", repr(up)
    upstep(up)
    return outs.toList

start = {rule="stm", at=1, up=null}

ins = HashMap.new
ins[start] = null
loop:
    //print "loop"
    var $processed = false
    ins.each: in, v ->
        if v: continue
        $processed = true
        ins[in] = true
        outs = trace(in)
        outs.each(out, n -> if not ins.has(out): ins[out] = null)
    if not $processed: break
print "DONE", ins.size, "states"
print "RECURSIONS", recpoints.size
recpoints.each: k, v ->
    print k, repr(v)
print "TERMINALS", terminals.size


lines = Array.new
lines.add "#include \"tlmeta.c\""
lines.add "#define GROW 2"
lines.add ""

emit_declare = rule ->
    lines.add "static State r_$(rule.name)(Parser*, int, int);"

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
    names[term.target].terms.first

emit = term ->
    { term.type == "call" }: "r_$(term.target)(_p, _pos, _start == _pos? _rec : 0);"
    { term.type == "prim" and try(term.text) }: "prim_$(term.target)(_p, _pos, \"$(term.text.escape(true))\");"
    { term.type == "prim" }: "prim_$(term.target)(_p, _pos);"
    { term.type == "name" }: emit(term.term)
    { term.type == "token"}: "state_ok(_pos, tlSTR(\"$(term.token.escape(true))\"));"
    { }: throw "ERROR emit:"+ repr(term)

var $nextcache = 1
rectokens = HashMap.new
rectoken = name ->
    /*
    under = name.find("_")
    name = { under }: name[:under - 1]
           {       }: name
    */
    if not rectokens[name]: rectokens[name] = rectokens.size
    1 << rectokens[name]

emit_rule = rule ->
    lines.add "static State r_$(rule.name)(Parser* _p, const int _start, int _rec) { // $(rule.type)"
    lines.add " print(\"START: $(rule.name) %d %d %c\", _start, _rec, _p->input[_start]);"
    cache = { rule.callers > 1      }: $nextcache += 1
            { directleft[rule.name] }: $nextcache += 1
            {                       }: 0
    if cache > 0:
        lines.add " State _c = cached(_p, _start, $cache);"
        lines.add " if (_c.ok) return parser_pass(_p, \"$(rule.name)\", _start, _c, 0);"
    lines.add " parser_enter(_p, \"$(rule.name)\", _start);"
    lines.add " int _pos = _start;"
    { can_optimize_and(rule) }:
        assert not directleft[rule.name]
        lines.add " State _r = "+ emit(rule.terms[1])
        lines.add " if (_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, _r, $cache);"
        lines.add " if (_p->error_line) { /*print(\"expect: $(rule.name)\");*/ return _r; }"
        lines.add " return parser_fail(_p, \"$(rule.name)\", _start);"
    { rule.type == "star" }:
        lines.add " tlArray* _v = tlArrayNew();"
        lines.add " State _r;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, _v), $cache);"
        lines.add " _pos = _r.pos;"
        lines.add " goto step1;"
    { rule.type == "starsep" }:
        lines.add " tlArray* _v = tlArrayNew();"
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, _v), $cache);"
        lines.add " _pos = _r.pos;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.sep)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, _v), $cache);"
        lines.add " _pos = _r.pos;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_fail(_p, \"$(rule.name)\", _start);"
        lines.add " _pos = _r.pos;"
        lines.add " goto step1;"
    { rule.type == "plus" }:
        lines.add " tlArray* _v = tlArrayNew();"
        lines.add " State _r;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_fail(_p, \"$(rule.name)\", _start);"
        lines.add " _pos = _r.pos;"
        lines.add "step1:;"
        lines.add " _r = "+ emit(rule.term)
        lines.add " if (!_r.ok) return parser_pass(_p, \"$(rule.name)\", _start, state_ok(_pos, _v), $cache);"
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
                lines.add " const int __indent = 0; //parser_indent(_p, _pos);"
                //lines.add " print(\"resetindent: %d from: %d\", __indent, _p->indent);"
                lines.add " const int _indent = _p->indent; _p->indent = __indent;"
                $indent = true
                return
            if term.type == "addindent":
                lines.add " const int __indent = parser_indent(_p, _pos);"
                //lines.add " print(\"addindent?: %d from: %d\", __indent, _p->indent);"
                lines.add " if (_p->indent >= __indent) return parser_fail(_p, \"$(rule.name)\", _pos);"
                lines.add " const int _indent = _p->indent; _p->indent = __indent;"
                $indent = true
                return
            if term.type == "setindent":
                lines.add " const int __indent = parser_indent(_p, _pos);"
                //lines.add " print(\"setindent?: %d from: %d\", __indent, _p->indent);"
                lines.add " if (_p->indent > __indent) return parser_fail(_p, \"$(rule.name)\", _pos);"
                lines.add " const int _indent = _p->indent; _p->indent = __indent;"
                $indent = true
                return
            if term.type == "indent":
                restore = { $indent }: "_p->indent = _indent; "
                          {         }: ""
                //lines.add " print(\"checking indent: %d (current: %d)\", parser_indent(_p, _pos), _p->indent);"
                lines.add " if (_p->indent != parser_indent(_p, _pos)) { $(restore)return parser_fail(_p, \"$(rule.name)\", _pos); }"
                return
            if term.type == "anchor":
                { not $anchor }: lines.add " const char* _anchor = parser_set_anchor(_p, \"$(term.msg.escape(true))\");"
                {             }: lines.add " _anchor = parser_set_anchor(_p, \"$(term.msg.escape(true))\");"
                $anchor = true
                return
            lines.add " _r = "+ emit(term)
            restore = { $indent }: "_p->indent = _indent; "
                      {         }: ""
            if not $anchor:
                lines.add " if (!_r.ok) { $(restore)return parser_fail(_p, \"$(rule.name)\", _pos); }"
            if $anchor:
                lines.add " if (!_r.ok) { $(restore)return parser_error(_p, \"$(rule.name)\", _start, _pos); }"
            lines.add " _pos = _r.pos;"
            if term.type == "name":
                names.add term.name
                lines.add " tlHandle $(term.name) = _r.value;"
        { try(rule.act) }:
            lines.add " tlHandle _v = $(rule.act);"
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

//io.File("calc.c").write(ast.before + lines.join("\n") + ast.after)
io.File("calc.c").write(lines.join("\n"))

