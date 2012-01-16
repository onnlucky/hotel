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
            trace(in, "_pmany: << $(res.size), noreturn: $(e?noreturn)")
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
    // TODO instead of just the last one, collect all alternatives
    var $err = null
    loop: (
        parser = args.get($i)
        if not parser: break
        trace(in, "pOr: $name: $($i)")
        $i += 1
        _catch: (e ->
            trace(in, "ERROR: $e NoReturn: $($nr)")
            if e?t != #p: _throw e
            if $nr: (
                if e?noreturn: _throw e
                _throw { t: #p, in: e.in, noreturn: true, msg: $nr }
            )
            if e?noreturn: $err = e
        )
        // TODO return parser(in) should work somehow ...
        in, v = parser(in)
        trace(in, "pOr: $name <<")
        return in, v
    )
    trace(in, "pOr: $name: no alternatives")
    if $err: _throw $err
    _throw { t: #p, in: in, msg: "a $name" }
)

lineFromPos = (text, pos ->
    // TODO bit inefficient ... but works
    lines = text.slice(0, pos).split("\n")
    var $last = lines.size - 1
    loop: (
        if $last < 0: return 1
        line = lines.get($last)
        // TODO use strip ...
        if line/*.strip*/.size == 0: $last -= 1; continue
        return $last + 1, line.size + 1
    )
)

pStart = (rule, text ->
    in, v = (
        _catch: (e ->
            if e?t != #p: _throw e
            line, char = lineFromPos(e.in.text, e.in.pos)
            if e?msg: _throw "SyntaxError: expected $(e.msg) on line $line col $char"
            _throw "SyntaxError: on line $line col $char"
        )
        rule({text: text, pos: 0 })
    )
    if in.pos < in.text.size: (
        line = lineFromPos(e.in.text, e.in.pos)
        _throw "SyntaxError: expected end of input on line $line col $char"
    )
    return v
)

