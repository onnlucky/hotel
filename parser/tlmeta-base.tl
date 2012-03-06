DEBUG=false
DEBUG2=false
trace = { in, &msg -> if DEBUG: print in.pos, in.text.escape.slice(in.pos, in.pos + 10), "--", msg() }
trace2 = { in, &msg -> if DEBUG2: print in.pos, in.text.escape.slice(in.pos, in.pos + 10), "--", msg() }

WS = " \t\n\r"
LOWER = "abcdefghijklmnopqrstuvwxyz"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ALPHA = LOWER.cat(UPPER)
NUMERIC = "0123456789"
ALPHANUMERIC = ALPHA.cat(NUMERIC)

advance = { in, adv -> { text: in.text, pos: in.pos + adv }}
error = { in, nr, msg ->
    if nr: _throw { t: #p, in: in, msg: nr, noreturn: true }
    _throw {t: #p, in: in, msg: msg }
}

rule_pass = { in, nr ->
    trace2(in, "pass")
    return in
}
rule_any = { in, nr ->
    trace2(in, "any")
    c = in.text.get(in.pos)
    if not c: error(in, nr, "eof")
    return advance(in, 1), c
}
rule_end = { in, nr ->
    trace2(in, "end")
    c = in.text.get(in.pos)
    if c: error(in, nr, "end")
    return in, null
}
pText = { in, nr, text ->
    trace(in, " >>  TRY \"$(text.escape)\"")
    if in.text.startsWith(text, in.pos): {
        trace(in, " << !OK!");
        return advance(in, text.size), text
    }
    trace(in, " << FAIL");
    error(in, nr, text)
}
pChar = { in, nr, text ->
    trace2(in, "pChar: $text")
    c = in.text.get(in.pos)
    if not c: error(in, nr, "[$text]")
    if text.search(c.toChar): return advance(in, 1), c
    error(in, nr, "[$text]")
}
pNot = { in, nr, parser ->
    trace2(in, "pNot")
    in = (
        _catch: { e =>
            if e?t != #p: _throw e
            if e?noreturn: _throw e
            return in, null
        }
        parser(in)
    )
    error(in, nr, "not")
}
pAhead = { in, nr, parser ->
    trace2(in, "pAhead")
    in2 = parser(in, nr)
    return in
}
pOpt = { in, nr, parser ->
    trace2(in, "pOpt")
    _catch: { e =>
        if e?t != #p: _throw e
        return in, null
    }
    parser(in)
}
_pmany = { in, parser, res ->
    trace2(in, "_pmany: $(res.size)")
    _catch: { e =>
        if e?t != #p: _throw e
        if e?noreturn: _throw e
        trace2(in, " << pStar|pPlus");
        return in, res
    }
    in, v = parser(in)
    goto _pmany(in, parser, res.add(v))
}
pStar = { in, nr, parser ->
    trace2(in, " >> pStar")
    _pmany(in, parser, [])
}
pPlus = { in, nr, parser ->
    trace2(in, " >> pPlus")
    in, v = parser(in, nr)
    _pmany(in, parser, [v])
}
pOr = { in, nr, name ->
    var $i = 3
    // TODO instead of just the last one, collect all alternatives
    loop: {
        parser = args.get($i)
        if not parser: break
        trace2(in, "pOr: $name: $($i)")
        $i += 1
        _catch: { e =>
            if e?t != #p: _throw e
            if e?noreturn: _throw e
        }
        in, v = parser(in)
        trace2(in, "pOr: $name <<")
        return in, v
    }
    trace2(in, "pOr: $name: no alternatives")
    error(in, nr, "a $name")
}

pHandle = { e, nr, name ->
    if e?t != #p: _throw e
    trace2(e.in, " << FAIL: $name")
    if e?noreturn: _throw e
    error(e.in, nr, e.msg)
}
pAct = { in, v -> return in, v }

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

rule_line = in -> return in, lineFromPos(in.text, in.pos)
rule_column = in -> return in, colFromPos(in.text, in.pos)

rule_lower = in, nr -> pChar(in, nr, LOWER)
rule_upper = in, nr -> pChar(in, nr, UPPER)
rule_alpha = in, nr -> pChar(in, nr, ALPHA)
rule_numeric = in, nr -> pChar(in, nr, NUMERIC)
rule_alphanumeric = in, nr -> pChar(in, nr, ALPHANUMERIC)

rule_ws = { in, nr ->
    trace2(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.search(c.toChar): goto(rule_ws({text: in.text, pos: in.pos + 1 }))
    return in, null
}

rule_wsnl = { in ->
    trace(in, "rule_ws")
    c = in.text.get(in.pos)
    if not c: return in, null
    if WS.search(c.toChar): goto(rule_ws({text: in.text, pos: in.pos + 1 }))
    return in, null
}

