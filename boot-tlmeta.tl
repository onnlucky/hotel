DEBUG=false
trace = in, &msg -> if DEBUG: print in.pos, in.item, msg()
DEBUG2=false
trace2 = in, &msg ->
    if DEBUG2: print in.pos, in.item, msg()
    return in

lower = "abcdefghijklmnopqrstuvwxyz"
upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
letters = lower.cat(upper)
numbers = "0123456789"

Input = { foo = true }
// TODO should be a class/object but class needs constructor and constructor needs class ...
input = text, pos, value -> {
        text = text
        pos = pos
        value = value
        item = text[pos]
        end = -> this.pos > this.text.size
        set = val -> input(this.text, this.pos, val)
        do = block -> this.set(block(this.value))
        next = val -> input(this.text, this.pos + 1, val)
        error = val -> throw this.set(val)
        class = Input
    }

// TODO this is crappy, use args.slice and .call ... but cannot quite do that yet?
rule = fn -> fn = args.block or fn; (a1, a2, a3, a4, a5, a6 -> (in -> in = args.block or in; fn(in, a1, a2, a3, a4, a5, a6)))

parse = parser, text ->
    out = (
        catch: e ->
            if e?class == Input: throw "syntax error: ".cat(e.value)
            throw e
        parser(input(text, 1, null))
    )
    if not out.end: throw "expected end of input"
    return out.value

pNot = rule(in, parser ->
    trace(in, "pNot")
    out = (
        catch: e ->
            if e?class == Input: trace(in, "pNot: OK: $(e)"); return in
            throw e
        parser(in)
    )
    trace(out, "pNot: Error")
    throw out
)
pOptional = rule(in, parser ->
    trace(in, "pOptional")
    catch: e ->
        if e?class == Input: trace(in, "ERROR: $e"); return in.set(null)
        throw e
    in = parser(in)
)
// returns only last result; make subrule if semantic actions are needed
pAnd = rule(in ->
    var $res = in
    var $i = 2
    loop:
        parser = args[$i]
        if not parser: trace(in, "pAnd: done $($i)"); return $res
        trace(in, "pAnd: $($i) of $(args.size)")
        $i += 1
        $res = parser($res)
)
pOr = rule(in ->
    var $i = 2
    loop:
        parser = args[$i]
        if not parser: break
        trace(in, "pOr: $($i)")
        $i += 1
        catch: e ->
            trace(in, "ERROR: $e")
            if e?class != Input: throw e
        return parser(in)
    trace(in, "no alternatives ...")
    in.error("no alternative")
)
_pmany = in, parser, res ->
    trace(in, "_pmany")
    catch: e ->
        if e?class == Input: trace(in, "ERROR: $e"); return in.set(res)
        throw e
    in = parser(in)
    goto _pmany(in, parser, res.add(in.value))

pMany = rule(in, parser ->
    trace(in, "pMany")
    _pmany(in, parser, [])
)
pManyOne = rule(in, parser ->
    trace(in, "pManyOne")
    in = parser(in)
    _pmany(in, parser, [in.value])
)
pChar = rule(in, ch ->
    trace(in, "pChar: $(in.item) == $(ch)")
    if in.item == ch: return in.next(ch)
    in.error(ch.toChar)
)
pChars = rule(in, chars ->
    trace(in, "pChars: $(in.item) in $(chars)")
    if not in.item: in.error(chars)
    if chars.find(in.item.toChar): return in.next(in.item)
    in.error(chars)
)
pNotChars = rule(in, chars ->
    trace(in, "pNotChars: $(in.item) not in $(chars)")
    if not in.item: in.error("not $chars")
    if chars.find(in.item.toChar): return in.error("not $chars")
    in.next(in.item)
)
pText = rule(in, text ->
    trace(in, "pText: $(text)")
    if in.text.startsWith(text, in.pos): return input(in.text, in.pos + text.size, text)
    in.error(text)
)
parseAny = in ->
    trace(in, "parseAny")
    if in.end: in.error("any")
    in.next(in.item)

parseEnd = in ->
    trace(in, "pEnd $(in.end), $(in.item)")
    if in.end: return in
    in.error("end of input")

parseWs = in -> trace(in, "."); pMany(pChars(" \t\n\r"))(in)
parseSpace = in -> pChars(" \t")(in)
parseNewline = in ->
    try: return pText("\r\n")(in)
    try: return pText("\n\r")(in)
    try: return pText("\n")(in)
    pText("\r")(in)

// PARSER PARSER

// . parser . end
//start: b=(!(name ."="."parser"."{") _)*
//       p=parser
//       a=_*                         -> { before: b.toChar, after: a.toChar, rules: p.rs, name: p.n }
parseStart = in ->
    trace2(in, "start")
    before = pMany(pAnd(
      pNot((in ->
        in = parseName(in)
        in = parseWs(in)
        in = pText("=")(in)
        in = parseWs(in)
        in = pText("parser")(in)
        in = parseWs(in)
        in = pText("{")(in)
      )),
      parseAny
    ))(in).do: v -> v.toChar
    parser = parseParser(before)
    after = pMany(parseAny)(parser).do: v -> v.toChar
    after.set({
        name = parser.value.n
        rules = parser.value.rs
        before = before.value
        after = after.value
    })

//parser: n=name ."="."parser"."{"
//        rs=rule*
//        ."}".                        -> { n: n, rs: rs }
parseParser = in ->
    trace2(in, "parser")
    name = parseName(in)
    in = parseWs(name)
    in = pText("=")(in)
    in = parseWs(in)
    in = pText("parser")(in)
    in = parseWs(in)
    in = pText("{")(in)
    in = parseWs(in)
    rules = pMany(parseRule)(in)
    in = parseWs(rules)
    in = pText("}")(in).set({ n = name.value, rs = rules.value })
    trace2(in, "<< parser: $(rules.value.size)")

//rule: n=name .":". t=or               -> { type: "Rule", name: n, term: t }
parseRule = in ->
    trace2(in, ">> rule")
    name = parseName(in)
    in = parseWs(name)
    in = pText(":")(in)
    in = parseWs(in)
    term = parseOr(in)
    in = parseWs(term).set({ type = "Rule", name = name.value, term = term.value })
    trace2(in, "<< rule: $(name.value)")

//act: "->". (!"\n" _)*
parseAct = in ->
    trace2(in, ">> action")
    in = parseWs(in)
    in = pText("->")(in)
    in = parseWs(in)
    cs = pMany(pNotChars("\n\r"))(in).do: v -> v.toChar
    trace2(cs, "<< action")

//or: h=and rs=(."|". and)*           -> { type: "Or", terms: [h].cat(rs) }
parseOr = in ->
    trace2(in, ">> or")
    h = parseAnd(in)
    rs = pMany(pAnd(parseWs, pText("|"), parseWs, parseAnd))(h)
    in = rs.set({ type = "Or", terms = [h.value].cat(rs.value) })
    trace2(in, "<< or")

//and: h=named rs=(" ". named)* .a=act -> { type: "And", terms: [h].cat(rs), act: a }
parseAnd = in ->
    trace2(in, ">> AND >> AND >> AND >>")
    h = parseNamed(in)
    rs = pMany(pAnd(parseWs, parseNamed))(h)
    a = pOptional(parseAct)(rs)
    in = a.set({ type = "And", terms = [h.value].cat(rs.value), act = a.value })
    trace2(in, "<< AND << AND << AND <<")

//named: n=name "=" t=post             -> { type: "Bind", name: n, term: t }
//     | "@" t=text                    -> { type: "NoReturn", msg: t }
//     | post
parseNamed = in ->
    trace2(in, ">> named")
    a1 = in ->
        n = parseName(in)
        in = pText("=")(n)
        parsePost(in).do: v -> { type = "Bind", name = n.value, term = v }
    a2 = in ->
        in = pText("@")(in)
        parseText(in).do: v -> { type = "NoReturn", msg = v }
    in = pOr(a1, a2, parsePost)(in)
    trace2(in, "<< named")

//post: "!" t=term                      -> { type: "Not", term: t }
//    | t=term "*"                      -> { type: "Star", term: t }
//    | t=term "+"                      -> { type: "Plus", term: t }
//    | t=term "?"                      -> { type: "Opt", term: t }
//    | term
parsePost = in ->
    trace2(in, ">> post")
    a1 = in -> in = pText("!")(in); parseTerm(in).do: v -> { type = "Not", term = v }
    a2 = in -> term = parseTerm(in); pText("*")(term).set({ type = "Star", term = term.value })
    a3 = in -> term = parseTerm(in); pText("+")(term).set({ type = "Plus", term = term.value })
    a4 = in -> term = parseTerm(in); pText("?")(term).set({ type = "Opt", term = term.value })
    in = pOr(a1, a2, a3, a4, parseTerm)(in)
    trace2(in, "<< post: $(in.value)")

//term: "(". t=or .")"                  -> { type: "Sub", term: t }
//    | "."                             -> { type: "Ws" }
//    | "_"                             -> { type: "Any" }
//    | n=name !":"                     -> { type: "Call", rule: n }
//    | t=text                          -> { type: "Text", text: t }
parseTerm = in ->
    trace2(in, ">> term")
    a1 = in ->
        in = pText("(")(in)
        trace2(in, ">> paren >> PAREN >> PAREN >>")
        catch: e -> print "!! !! SYNTAX ERROR::", e, ":: !! !!"
        in = parseWs(in)
        t=parseOr(in)
        in = parseWs(t)
        in = pText(")")(in).set({ type = "Sub", term = t.value })
        trace2(in, "3 << paren << PAREN << PAREN <<")

    a2 = in -> pText(".")(in).set({ type = "Ws" })
    a3 = in -> pText("_")(in).set({ type = "Any" })
    a4 = in -> n = parseName(in); pNot(pChar(":".get))(n).set({ type = "Call", rule = n.value })
    a5 = in -> parseText(in).do: v -> { type = "Text", text = v }
    in = pOr(a1, a2, a3, a4, a5)(in)
    trace2(in, "<< term: $(in.value)")

//text: "\"" escape* "\""
parseText = in ->
    trace2(in, "text")
    in = pText("\"")(in)
    cs = pMany(parseEscape)(in).do: v -> v.join
    pText("\"")(cs).set(cs.value)

//escape: "\\\"" | "\\n" | "\\r" | "\\t" | "\\\\" | !"\"" _
parseEscape = in ->
    trace2(in, "escape")
    a1 = in -> pText("\\\"")(in).set("\"")
    a2 = in -> pText("\\n")(in).set("\n")
    a3 = in -> pText("\\r")(in).set("\r")
    a4 = in -> pText("\\t")(in).set("\t")
    a5 = in -> pText("\\\\")(in).set("\\")
    a6 = in -> pNotChars("\"")(in).do: v -> v.toChar
    pOr(a1, a2, a3, a4, a5, a6)(in)

//name: ls=letters+ -> ls.join
parseName = in ->
    trace2(in, ">> name")
    in = pManyOne(pChars(letters))(in).do: v -> v.toChar
    trace2(in, "<< name: $(in.value)")


text = io.File("tlmeta.tlg").readString
ast = parse(parseStart, text)
//print inspect(ast)

// ** emitting a new parser **
emit = r, buf, ind ->
    { r.type == "Or"  }: goto emitOr r, buf, ind
    { r.type == "And" }: goto emitAnd r, buf, ind
    { r.type == "Sub" }: goto emitSub r, buf, ind
    { r.type == "Not" }: goto emitNot r, buf, ind
    { r.type == "Opt" }: goto emitOpt r, buf, ind
    { r.type == "Star"}: goto emitStar r, buf, ind
    { r.type == "Plus"}: goto emitPlus r, buf, ind

    { r.type == "NoReturn"}: buf.write("$ind  _nr = \"$(r.msg.escape)\"\n")
    { r.type == "Call"    }: buf.write("$ind  _in, _v = rule_$(r.rule)(_in, _nr)\n")
    { r.type == "Bind"    }: emit(r.term, buf, ind); buf.write("$(ind)  $(r.name) = _v; pAct(_in, _v)\n")
    { r.type == "Text"    }: buf.write("$ind  _in, _v = pText(_in, _nr, \"$(r.text.escape)\")\n")
    { r.type == "Ws"      }: buf.write("$ind  _in, _v = rule_ws(_in, _nr)\n")
    { r.type == "Any"     }: buf.write("$ind  _in, _v = rule_any(_in, _nr)\n")
    { true                }: throw "InternalError emit unknown: $(r.type)"

emitNot = r, buf, ind ->
    buf.write("$ind  _in, _v = pNot(_in, _nr, (_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")

emitOpt = r, buf, ind ->
    buf.write("$ind  _in, _v = pOpt(_in, _nr, (_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")

emitStar = r, buf, ind ->
    buf.write("$ind  _in, _v = pStar(_in, _nr, (_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")

emitPlus = r, buf, ind ->
    buf.write("$ind  _in, _v = pPlus(_in, _nr, (_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  ))\n")

emitAnd = r, buf, ind ->
    // TODO maybe collect all binds, and "call" r.act
    r.terms.each: r -> emit r, buf, ind
    if r.act: buf.write("$ind  _in, _v = pAct(_in, ($(r.act)))\n")

emitOr = r, buf, ind ->
    if r.terms.size == 0: return
    if r.terms.size == 1: return emit(r.terms[1], buf, ind)

    buf.write("$ind  _in, _v = pOr(_in, _nr, _name\n")
    r.terms.each: r ->
        buf.write("$ind    ,(_in, _nr ->\n")
        emit r, buf, ind.cat("    ")
        buf.write("$ind     )\n")
    buf.write("$ind  )\n")

emitSub = r, buf, ind ->
    buf.write("$ind  _in, _v = (_in, _nr ->\n")
    emit r.term, buf, ind.cat("  ")
    buf.write("$ind  )(_in, _nr)\n")

emitRule = r, buf ->
    name = r.name
    buf.write("rule_$name = (_in, _nr ->\n")
    buf.write("  trace(_in, \" >>  TRY $name\", _nr)\n")
    buf.write("  this = _in.oop\n")
    buf.write("  _name = \"$name\"\n")
    buf.write("  _pos = _in.pos\n")
    buf.write("  _hit = _tllookup(this, _name, _pos)\n")
    buf.write("  if _hit: return _hit.in, _hit.v\n")
    buf.write("  catch: e -> pHandle(e, _nr, _name)\n")
    emit r.term, buf, ""
    buf.write("  trace(_in, \" << !OK! $name\")\n")
    buf.write("  goto _tlcache(this, _name, _pos, _in, _v)\n")
    buf.write(")\n")


buf = io.Buffer.new
buf.write(ast.before)
buf.write("\n// --- tlmeta-base.tl ---\n")
buf.write(io.File("tlmeta-base.tl").readString)
buf.write("\n// --- parser ---\n")
var $hasstart = false
ast.rules.each: r ->
    if r.name == "start": $hasstart = true
    emitRule r, buf
start = (if $hasstart: "start") or ast.rules[1].name
buf.write("$(ast.name)$("") = text -> pStart(rule_$start, text)\n")
buf.write("\n// ---\n")
buf.write(ast.after)

io.File("tlmeta").write(buf)
print "written 'tlmeta'"

