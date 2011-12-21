// http request and url parser

"echo hello world!".split.join(", ")

/*
DEBUG=false
trace = (in, &msg -> if DEBUG: print in.pos, in.item, msg())

lower = "abcdefghijklmnopqrstuvwxyz"
upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
numbers = "0123456789"

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
    }
)
// TODO this is crappy, use args.slice and .call ... but cannot quite do that yet?
rule = (fn -> (a1, a2, a3, a4 -> (in -> fn(in, a1, a2, a3, a4))))

parse = (parser, text ->
    out = (
        _catch: e -> _throw "syntax error"
        parser(input(text, 0, null))
    )
    if not out.end: _throw "expected end of input"
    return out.value
)

pOptional = rule((in, parser ->
    trace(in, "pOptional")
    _catch: e -> trace(in, "ERROR: $e"); return in.set(null)
    in = parser(in)
))
// TODO this rule is not very useful because it does not allow for semantic actions
// TODO should return [value] ... instead
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
    var $i = 0
    while $i < args.size - 1: (
        $i += 1
        _catch: e -> trace(in, "ERROR: $e")
        trace(in, "pOr: try $($i) of $(args.size)")
        return args.get($i)(in)
    )
    trace(in, "no alternatives ...")
    in.error("no alternative")
))
_pmany = (in, parser, res ->
    trace(in, "_pmany")
    _catch: e -> trace(in, "ERROR: $e"); return in.set(res)
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
parseSpace = (in -> pChars(" \t")(in))
parseWhitespace = (in -> pChars(" \t\n\r")(in))
parseNewline = (in ->
    try: return pText("\r\n")(in)
    try: return pText("\n\r")(in)
    try: return pText("\n")(in)
    pText("\r")(in)
)

// DOC PARSING

doCmd = (cmd, text ->
    if cmd == "title": return "<h1>$text</h1>"
    "<$cmd>$text</$cmd>"
)

parseLine = (in ->
    line = pManyOne(pNotChars("\r\n"))(in).do: v -> v.toChar
    nl = pOr(parseNewline,parseEnd)(line).set(line.value)
)
parsePara = (in ->
    lines = pManyOne(parseLine)(in)
    nl = pOr(parseNewline, parseEnd)(lines).set("<p>\n$(_list_join(lines.value, "\n"))\n</p>")
)
parseCmd = (in ->
    trace(in, "parseCmd")
    in = pChar("/".char)(in)
    cmd = pManyOne(pNotChars(": \t\r\n"))(in).do: v -> v.toChar
    in = pChar(":".char)(cmd)
    in = pMany(parseSpace)(in)
    text = parseLine(in)
    text.set(doCmd(cmd.value, text.value))
)
parseBlock = (in ->
    res = pOr(parseCmd, parsePara)(in)
    pMany(parseWhitespace)(res).set(res.value)
)
parseDoc = (in ->
    in = pMany(parseWhitespace)(in)
    pMany(parseBlock)(in)
)

text = io.File(args.get(0)).read
_list_join(parse(parseDoc, text), "\n")

