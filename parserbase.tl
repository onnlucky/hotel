DEBUG=false
trace = (in, &msg -> if DEBUG: print in.pos, in.text.slice(in.pos, in.pos + 10).escape, "--", msg())

WS = " \t\n\r"
LOWER = "abcdefghijklmnopqrstuvwxyz"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ALPHA = LOWER.cat(UPPER)
NUMBER = "0123456789"

pAny = (in ->
    trace(in, "pAny")
    c = in.text.get(in.pos)
    if not c: _throw { t: #p, in: in }
    return { text: in.text, pos: in.pos + 1 }, c
)
pText = (in, text ->
    trace(in, "pText: $text")
    if in.text.startsWith(text, in.pos): return { text: in.text, pos: in.pos + text.size }, text
    _throw { t: #p, in: in }
)

rule_alpha = (in ->
    trace(in, "rule_alpha")
    c = in.text.get(in.pos)
    if not c: _throw { t: #p, in: in }
    if ALPHA.search(c.toChar): return { text: in.text, pos: in.pos + 1 }, c
    _throw { t: #p, in: in }
)
rule_ws = (in ->
    trace(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.search(c.toChar): goto(rule_ws({text: in.text, pos: in.pos + 1 }))
    return in, null
)

pNot = (in, parser ->
    trace(in, "pNot")
    in = (
        _catch: (e ->
            if e?t == #p: (
                if e?noreturn: _throw e
                return in, null
            )
            _throw e
        )
        parser(in)
    )
    _throw { t: #p, in: in }
)
pOpt = (in, parser ->
    trace(in, "pOpt")
    _catch: (e ->
        if e?t == #p: (
            if e?noreturn: _throw e
            return in, null
        )
        _throw e
    )
    parser(in)
)
_pmany = (in, parser, res ->
    trace(in, "_pmany: $(res.size)")
    _catch: (e ->
        if e?t == #p: (
            if e?noreturn: _throw e
            return in, res
        )
        _throw e
    )
    in, v = parser(in)
    goto _pmany(in, parser, res.add(v))
)
pStar = (in, parser ->
    trace(in, "pStar")
    _pmany(in, parser, [])
)
pPlus = (in, parser ->
    trace(in, "pPlus")
    in, v = parser(in)
    _pmany(in, parser, [v])
)
pOr = (name, nr, in ->
    var $i = 3
    // TODO instead of just one, collect all alternatives
    var $err = null
    loop: (
        parser = args.get($i)
        if not parser: break
        trace(in, "pOr: $($i)")
        $i += 1
        _catch: (e ->
            trace(in, "ERROR: $e NoReturn: $($nr)")
            if e?t != #p: _throw e
            if $nr: _throw { t: #p, in: e.in, noreturn: true, msg: ($nr) or name }
            if e?noreturn: $err = e
        )
        // TODO return parser(in) should work somehow ...
        in, v = parser(in)
        return in, v
    )
    trace(in, "pOr: no alternatives")
    if $err: _throw $err
    // TODO add rule name here ...
    _throw { t: #p, in: in }
)

pStart = (rule, text ->
    in, v = (
        _catch: (e ->
            if e?t != #p: _throw e
            if e?msg: _throw "SyntaxError: near $(e.in.pos); expected a $(e.msg)"
            _throw "SyntaxError: near $(e.in.pos)"
        )
        rule({text: text, pos: 0 })
    )
    if in.pos < in.text.size: _throw "SyntaxError: expected EOF near $(in.pos)"
    return v
)
