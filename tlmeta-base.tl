DEBUG=false
trace = { in, &msg -> if DEBUG: print in.pos, in.text.slice(in.pos, in.pos + 10).escape, "--", msg() }

WS = " \t\n\r"
LOWER = "abcdefghijklmnopqrstuvwxyz"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ALPHA = LOWER.cat(UPPER)
NUMERIC = "0123456789"
ALPHANUMERIC = ALPHA.cat(NUMERIC)

pAny = { in ->
    trace(in, "pAny")
    c = in.text.get(in.pos)
    if not c: _throw { t: #p, in: in }
    return { text: in.text, pos: in.pos + 1 }, c
}
pText = { in, text ->
    trace(in, "pText: $text")
    if in.text.startsWith(text, in.pos): return { text: in.text, pos: in.pos + text.size }, text
    _throw { t: #p, in: in, msg: text }
}
pChar = { in, text ->
    trace(in, "pChar: $text")
    c = in.text.get(in.pos)
    if not c: _throw { t: #p, in: in }
    if text.search(c.toChar): return { text: in.text, pos: in.pos + 1 }, c
    _throw { t: #p, in: in }
}

pNot = { in, parser ->
    trace(in, "pNot")
    in = (
        _catch: { e =>
            if e?t == #p: (
                if e?noreturn: _throw e
                return in, null
            )
            _throw e
        }
        parser(in)
    )
    _throw { t: #p, in: in }
}
pAhead = { in, parser ->
    trace(in, "pAhead")
    in2 = parser(in)
    return in
}
pOpt = { in, parser ->
    trace(in, "pOpt")
    _catch: { e =>
        if e?t == #p: (
            if e?noreturn: _throw e
            return in, null
        )
        _throw e
    }
    parser(in)
}
_pmany = { in, parser, res ->
    trace(in, "_pmany: $(res.size)")
    _catch: { e ->
        if e?t == #p: (
            trace(in, "_pmany: << $(res.size), noreturn: $(e?noreturn)")
            if e?noreturn: _throw e
            return in, res
        )
        _throw e
    }
    in, v = parser(in)
    goto _pmany(in, parser, res.add(v))
}
pStar = { in, parser ->
    trace(in, "pStar")
    _pmany(in, parser, [])
}
pPlus = { in, parser ->
    trace(in, "pPlus")
    in, v = parser(in)
    _pmany(in, parser, [v])
}
pOr = { name, in ->
    var $i = 2
    // TODO instead of just the last one, collect all alternatives
    var $nr = null
    loop: {
        parser = args.get($i)
        if not parser: break
        trace(in, "pOr: $name: $($i)")
        $nr = null
        $i += 1
        _catch: { e =>
            trace(in, "ERROR: $e NoReturn: $($nr)")
            if e?t != #p: _throw e
            if e?noreturn: _throw e
            if $nr: _throw { t: #p, in: e.in, noreturn: true, msg: $nr }
        }
        in, v = parser(nr, in)
        trace(in, "pOr: $name <<")
        return in, v
    }
    trace(in, "pOr: $name: no alternatives")
    _throw { t: #p, in: in, msg: "a $name" }
}

pHandle = { e, nr ->
    if e?t != #p: _throw e
    if e?noreturn: _throw e
    if nr: _throw { t:#p, in: e.in, noreturn: true, msg: nr }
    _throw e
}

colFromPos = { text, pos ->
    var $col = 0
    loop: {
        at = pos - $col - 1
        if at < 0: return pos
        if text.startsWith("\n", at): return $col
        $col += 1
    }
}
lineFromPos = { text, pos ->
    // TODO bit inefficient ... but works
    lines = text.slice(0, pos).split("\n")
    var $last = lines.size - 1
    loop: {
        if $last < 0: return 1
        line = lines.get($last)
        if line.strip.size == 0: $last -= 1; continue
        return $last + 1, line.size + 1
    }
}

pStart = { rule, text ->
    in, v = (
        _catch: { e ->
            if e?t != #p: _throw e
            line, char = lineFromPos(e.in.text, e.in.pos)
            if e?msg: _throw "SyntaxError: expected $(e.msg) on line $line col $char"
            _throw "SyntaxError: on line $line col $char"
        }
        rule({text: text, pos: 0 })
    )
    if in.pos < in.text.size: {
        line, char = lineFromPos(in.text, in.pos)
        _throw "SyntaxError: expected end of input on line $line col $char"
    }
    return v
}

rule_end = { in ->
    c = in.text.get(in.pos)
    if c: _throw { t: #p, in: in }
    return in, null
}

rule_line = in -> return in, lineFromPos(in.text, in.pos)
rule_column = in -> return in, colFromPos(in.text, in.pos)

rule_lower = in -> pChar(in, LOWER)
rule_upper = in -> pChar(in, UPPER)
rule_alpha = in -> pChar(in, ALPHA)
rule_numeric = in -> pChar(in, NUMERIC)
rule_alphanumeric = in -> pChar(in, ALPHANUMERIC)

rule_ws = { in ->
    trace(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.search(c.toChar): goto(rule_ws({text: in.text, pos: in.pos + 1 }))
    return in, null
}

