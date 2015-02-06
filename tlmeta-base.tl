DEBUG=false
DEBUG2=false
trace = in, &msg -> if DEBUG: print in.pos, in.text.escape[in.pos:in.pos + 10], "--", msg()
trace2 = in, &msg -> if DEBUG2: print in.pos, in.text.escape[in.pos:in.pos + 10], "--", msg()

WS = " \t\n\r"
LOWER = "abcdefghijklmnopqrstuvwxyz"
UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ALPHA = LOWER.cat(UPPER)
NUMERIC = "0123456789"
ALPHANUMERIC = ALPHA.cat(NUMERIC)

advance = in, adv -> { text = in.text, pos = in.pos + adv, oop = in.oop }
error = in, nr, msg ->
    if nr: throw { t = "p", in = in, msg = nr, noreturn = true }
    throw { t = "p", in = in, msg = msg }

rule_pass = in, nr ->
    trace2(in, "pass")
    return in

rule_any = in, nr ->
    trace2(in, "any")
    c = in.text[in.pos]
    if not c: error(in, nr, "eof")
    return advance(in, 1), c

rule_end = in, nr ->
    trace2(in, "end")
    c = in.text[in.pos]
    if c: error(in, nr, "end")
    return in, null

pText = in, nr, text ->
    trace(in, " >>  TRY \"$(text.escape)\"")
    if in.text.startsWith(text, in.pos):
        trace(in, " << !OK!")
        return advance(in, text.size), text
    trace(in, " << FAIL")
    error(in, nr, text)

pChar = in, nr, text ->
    trace2(in, "pChar: $text")
    c = in.text[in.pos]
    if not c: error(in, nr, "[$text]")
    if text.find(c.toChar): return advance(in, 1), c
    error(in, nr, "[$text]")

pNot = in, nr, parser ->
    trace2(in, "pNot")
    in = (
        catch: e ->
            if try(e.t) != "p": throw e
            if try(e.noreturn): throw e
            return in, null
        parser(in)
    )
    error(in, nr, "not")

pAhead = in, nr, parser ->
    trace2(in, "pAhead")
    in2 = parser(in, nr)
    return in

pOpt = in, nr, parser ->
    trace2(in, "pOpt")
    catch: e ->
        if try(e.t) != "p": throw e
        if try(e.noreturn): throw e
        return in, null
    parser(in)

_pmany = in, parser, res ->
    trace2(in, "_pmany: $(res.size)")
    catch: e ->
        if try(e.t) != "p": throw e
        if try(e.noreturn): throw e
        trace2(in, " << pStar|pPlus")
        return in, res
    in, v = parser(in)
    goto _pmany(in, parser, res.add(v))

pStar = in, nr, parser ->
    trace2(in, " >> pStar")
    _pmany(in, parser, [])

pPlus = in, nr, parser ->
    trace2(in, " >> pPlus")
    in, v = parser(in, nr)
    _pmany(in, parser, [v])

pOr = in, nr, name ->
    var $i = 4
    # TODO instead of just the last one, collect all alternatives
    loop:
        parser = args[$i]
        if not parser: break
        trace2(in, "pOr: $name: $($i)")
        $i += 1
        catch: e ->
            if try(e.t) != "p": throw e
            if try(e.noreturn): throw e
        in, v = parser(in)
        trace2(in, "pOr: $name <<")
        return in, v
    trace2(in, "pOr: $name: no alternatives")
    error(in, nr, "a $name")

pHandle = e, nr, name ->
    if try(e.t) != "p": throw e
    trace(e.in, " << FAIL $name")
    if try(e.noreturn): throw e
    error(e.in, nr, e.msg)

pAct = in, v -> return in, v

colFromPos = text, pos ->
    var $col = 0
    loop:
        at = pos - $col - 1
        if at < 0: return pos
        if text.startsWith("\n", at): return $col
        $col += 1

lineFromPos = text, pos ->
    # TODO bit inefficient ... but works
    lines = text[:pos].split("\n")
    var $last = lines.size
    loop:
        if $last < 0: return 1
        line = lines[$last]
        if line.trim.size == 0: $last -= 1; continue
        return $last, line.size + 1

_tllookup = oop, name, pos ->
    hit = oop._cache["$name-$pos"]
    if hit:
        #print "HIT: $name $pos -> $(hit.in.pos) '$(hit.v)'"
        return hit
    return

_tlcache = oop, name, pos, in, v ->
    #print "CACHE: $name $pos -> $(in.pos) '$v'"
    oop._cache.set("$name-$pos", {in = in, v = v})
    return in, v

pStart = rule, text ->
    oop = _Mutable_new({ _cache = HashMap.new })
    _with_lock(oop):
        in, v = (
            catch: e ->
                if try(e.t) != "p": throw e
                line, char = lineFromPos(e.in.text, e.in.pos)
                if try(e.msg): throw "SyntaxError: expected $(e.msg) on line $line col $char"
                throw "SyntaxError: on line $line col $char"
            rule({text = text, pos = 1, oop = oop })
        )
        if in.pos < in.text.size:
            line, char = lineFromPos(in.text, in.pos)
            throw "SyntaxError: expected end of input on line $line col $char"
        return v

rule_line = in -> return in, lineFromPos(in.text, in.pos)
rule_column = in -> return in, colFromPos(in.text, in.pos)

rule_lower = in, nr -> pChar(in, nr, LOWER)
rule_upper = in, nr -> pChar(in, nr, UPPER)
rule_alpha = in, nr -> pChar(in, nr, ALPHA)
rule_numeric = in, nr -> pChar(in, nr, NUMERIC)
rule_alphanumeric = in, nr -> pChar(in, nr, ALPHANUMERIC)

rule_ws = in, nr ->
    trace2(in, "rule_ws")
    c = in.text[in.pos]
    if not c: return in, null
    if WS.find(c.toChar): goto(rule_ws(advance(in, 1)))
    return in, null

rule_wsnl = in ->
    trace(in, "rule_ws")
    c = in.text[in.pos]
    if not c: return in, null
    if WS.find(c.toChar): goto(rule_ws(advance(in, 1)))
    return in, null

