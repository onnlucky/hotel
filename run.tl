DEBUG=true
trace = (in, &msg -> if DEBUG: print in.pos, in.item, msg())
DEBUG2=true
trace2 = (in, &msg ->
    if DEBUG2: print in.pos, in.item, msg()
    return in
)

lower = "abcdefghijklmnopqrstuvwxyz"
upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
letters = lower.cat(upper)
numbers = "0123456789"

Input = { foo: true }
// TODO should be a class/object but class needs constructor and constructor needs class ...
input = (text, pos, value -> {
        text: text
        pos: pos
        value: value
        item: text.char(pos)
        end: -> this.pos >= this.text.size
        set: val -> input(this.text, this.pos, val)
        do: (block -> this.set(block(this.value)))
        next: val -> input(this.text, this.pos + 1, val)
        error: val -> _throw this.set(val)
        class: Input
    }
)
// TODO this is crappy, use args.slice and .call ... but cannot quite do that yet?
rule = (fn -> (a1, a2, a3, a4, a5, a6, a7, a8, a9 -> (in -> fn(in, a1, a2, a3, a4, a5, a6, a7, a8, a9))))

parse = (parser, text ->
    out = (
        _catch: (e ->
            if e?class == Input: _throw "syntax error: ".cat(e.value)
            _throw e
        )
        parser(input(text, 0, null))
    )
    if not out.end: _throw "expected end of input"
    return out.value
)
pNot = rule((in, parser ->
    trace(in, "pNot")
    out = (
        _catch: (e ->
            if e?class == Input: trace(in, "pNot: OK: $(e)"); return in
            _throw e
        )
        parser(in)
    )
    trace(out, "pNot: Error")
    _throw out
))
pOptional = rule((in, parser ->
    trace(in, "pOptional")
    _catch: (e ->
        if e?class == Input: trace(in, "ERROR: $e"); return in.set(null)
        _throw e
    )
    in = parser(in)
))
// returns only last result; make subrule if semantic actions are needed
pAnd = rule((in ->
    var $res = in
    var $i = 1
    loop: (
        parser = args.get($i)
        if not parser: trace(in, "pAnd: done $($i)"); return $res
        trace(in, "pAnd: $($i) of $(args.size)")
        $i += 1
        $res = parser($res)
    )
))
pOr = rule((in ->
    var $i = 1
    loop: (
        parser = args.get($i)
        if not parser: break
        trace(in, "pOr: $($i)")
        $i += 1
        _catch: (e ->
            trace(in, "ERROR: $e")
            if e?class != Input: _throw e
        )
        return parser(in)
    )
    trace(in, "no alternatives ...")
    in.error("no alternative")
))
_pmany = (in, parser, res ->
    trace(in, "_pmany")
    _catch: (e ->
        if e?class == Input: trace(in, "ERROR: $e"); return in.set(res)
        _throw e
    )
    in = parser(in)
    goto _pmany(in, parser, res.add(in.value))
)
pMany = rule((in, parser ->
    trace(in, "pMany")
    _pmany(in, parser, [])
))
pManyOne = rule((in, parser ->
    trace(in, "pManyOne")
    in = parser(in)
    _pmany(in, parser, [in.value])
))
pChar = rule((in, ch ->
    trace(in, "pChar: $(in.item) == $(ch)")
    if in.item == ch: return in.next(ch)
    in.error(ch.toChar)
))
pChars = rule((in, chars ->
    trace(in, "pChars: $(in.item) in $(chars)")
    if not in.item: in.error(chars)
    if chars.search(in.item.toChar): return in.next(in.item)
    in.error(chars)
))
pNotChars = rule((in, chars ->
    trace(in, "pNotChars: $(in.item) not in $(chars)")
    if not in.item: in.error("not $chars")
    if chars.search(in.item.toChar): return in.error("not $chars")
    in.next(in.item)
))
pText = rule((in, text ->
    trace(in, "pText: $(text)")
    if in.text.startsWith(text, in.pos): return input(in.text, in.pos + text.size, text)
    in.error(text)
))
parseEnd = (in ->
    trace(in, "pEnd $(in.end), $(in.item)")
    if in.end: return in
    in.error("end of input")
)
parseWs = (in -> trace(in, "."); pMany(pChars(" \t\n\r"))(in))
parseSpace = (in -> pChars(" \t")(in))
parseNewline = (in ->
    try: return pText("\r\n")(in)
    try: return pText("\n\r")(in)
    try: return pText("\n")(in)
    pText("\r")(in)
)

// PARSER PARSER

// . parser . end
parseOuter = (in ->
    trace2(in, "outer")
    in = parseWs(in)
    parser = parseParser(in)
    in = parseWs(parser)
    parseEnd(in)
)
// parser: "parser" ."{". rs=rule* ."}" -> Parser.{ rules: rs }
parseParser = (in ->
    trace2(in, "parser")
    in = pText("parser")(in)
    in = parseWs(in)
    in = pText("{")(in)
    in = parseWs(in)
    rules = pMany(parseRule)(in)
    in = parseWs(rules)
    in = pText("}")(in)
    trace2(in, "<< parser: $(rules.value.size)")
)
//rule: n=name .":". t=or .              -> Rule.{ name: n, term: t }
parseRule = (in ->
    trace2(in, ">> rule")
    name = parseName(in)
    in = parseWs(name)
    in = pText(":")(in)
    in = parseWs(in)
    term = parseOr(in)
    in = parseWs(term)
    trace2(in, "<< rule: $(name.value)")
)
//act: "->". (!"\n" _)*
parseAct = (in ->
    trace2(in, ">> action")
    in = parseWs(in)
    in = pText("->")(in)
    in = parseWs(in)
    cs = pMany(pNotChars("\n\r"))(in).do: v -> v.toChar
    trace2(cs, "<< action")
)
//or: h=and rs=(."|". and)*          -> Or.{ terms: h + rs }
parseOr = (in ->
    trace2(in, ">> or")
    h = parseAnd(in)
    rs= pMany(pAnd(parseWs, pText("|"), parseWs, parseAnd))(h)
    trace2(rs, "<< or")
)
//and: h=named rs=(" ". named)* . a:act? -> And.{ terms: h + rs, act: a }
parseAnd = (in ->
    trace2(in, ">> AND >> AND >> AND >>")
    h = parseNamed(in)
    rs = pMany(pAnd(parseWs, parseNamed))(h)
    a = pOptional(parseAct)(rs)
    trace2(a, "<< AND << AND << AND <<")
)
//named: n=name "=" t=post              -> Bind.{ name: n, term: t }
//     | post
parseNamed = (in ->
    trace2(in, ">> named")
    a1 = in -> n = parseName(in); in = pText("=")(n); t = parsePost(in)
    in = pOr(a1, parsePost)(in)
    trace2(in, "<< named")
)
//post: "!" term | term "*" | term "+" | term
parsePost = (in ->
    trace2(in, ">> post")
    a1 = in -> in = pText("!")(in); parseTerm(in).set("!!!")
    a2 = in -> term = parseTerm(in); pText("*")(term).set("***")
    a3 = in -> term = parseTerm(in); pText("+")(term).set("+++")
    in = pOr(a1, a2, a3, parseTerm)(in)
    trace2(in, "<< post: $(in.value)")
)
//term: "(". or .")" | "." | "_" | name | text
parseTerm = (in ->
    trace2(in, ">> term")
    a1 = (in ->
        in = pText("(")(in);
        trace2(in, ">> paren >> PAREN >> PAREN >>")
        _catch: e -> print "!! !! SYNTAX ERROR::", e, ":: !! !!"
        in = parseWs(in);
        t=parseOr(in);
        in = parseWs(t);
        in = pText(")")(in)
        trace2(in, "3 << paren << PAREN << PAREN <<")
    )
    a2 = in -> pText(".")(in)
    a3 = in -> pText("_")(in)
    a4 = in -> in = parseName(in); pNot(pChar(":".char))(in)
    in = pOr(a1, a2, a3, a4, parseText)(in)
    trace2(in, "<< term: $(in.value)")
)
//text: "\"" escape* "\""
parseText = (in ->
    trace2(in, "text")
    in = pText("\"")(in)
    cs = pMany(parseEscape)(in)
    pText("\"")(cs)
)
//escape: "\\\"" | "\\n" | "\\r" | "\\t" | "\\\\" | !"\"" _
parseEscape = (in ->
    trace2(in, "escape")
    a1 = in -> pText("\\\"")(in).set("\"".char)
    a2 = in -> pText("\\n")(in).set("\n".char)
    a3 = in -> pText("\\r")(in).set("\r".char)
    a4 = in -> pText("\\t")(in).set("\t".char)
    a5 = in -> pText("\\\\")(in).set("\\".char)
    a6 = in -> pNotChars("\"")(in)
    pOr(a1, a2, a3, a4, a5, a6)(in)
)
//name: letters*
parseName = (in ->
    trace2(in, ">> name")
    in = pManyOne(pChars(letters))(in)//.do: v -> v.toChar
    trace2(in, "<< name: $(in.value)")
)

text = io.File("parser.tl").read
parse(parseOuter, text)
//parse(parseParser, "parser{a:b c:a}")

