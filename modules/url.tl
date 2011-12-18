// a url parser
DEBUG=false
trace = (in, &msg -> if DEBUG: print in.pos, msg())

// TODO should be a class/object but class needs constructor and constructor needs class ...
input = (text, pos, value -> {
        text: text
        pos: pos
        value: value
        item: text.char(pos)
        end: -> this.pos >= this.text.size
        set: val -> input(this.text, this.pos, val)
        next: val -> input(this.text, this.pos + 1, val)
        error: val -> _throw this.set(val)
    }
)

parse = (text, parser ->
    in = parser(input(text, 0, null))
    if not in.end: _throw "expected end of input"
    return in.value
)
parseOptional = (in, parser, a1, a2, a3 ->
    trace(in, "parseOptional")
    _catch: e -> trace(in, "ERROR: $e"); return in.set(null)
    in = parser(in, a1, a2, a3)
)
_pmany = (in, parser, res, a1, a2, a3 ->
    trace(in, "_pmany")
    _catch: e -> trace(in, "ERROR: $e"); return in.set(res)
    in = parser(in, a1, a2, a3)
    goto _pmany(in, parser, res.add(in.value), a1, a2, a3)
)
parseMany = (in, parser, a1, a2, a3 ->
    trace(in, "parseMany")
    _pmany(in, parser, [], a1, a2, a3)
)
parseManyOne = (in, parser, a1, a2, a3 ->
    trace(in, "parseManyOne")
    in = parser(in, a1, a2, a3)
    _pmany(in, parser, [in.value], a1, a2, a3)
)
parseChar = (in, ch ->
    trace(in, "parseChar: $(in.item.toChar) == $(ch.toChar)")
    if in.item == ch: return in.next(ch)
    in.error(ch.toChar)
)
parseChars = (in, chars ->
    trace(in, "parseChars: $(in.item.toChar) in $(chars)")
    if chars.search(in.item.toChar): return in.next(in.item)
    in.error(chars)
)
parseNotChars = (in, chars ->
    trace(in, "parseNotChars: $(in.item.toChar) not in $(chars)")
    if chars.search(in.item.toChar): return in.error("not $chars")
    in.next(in.item)
)
parseText = (in, text ->
    trace(in, "parseText: $(text)")
    if in.text.startsWith(text, in.pos): return input(in.text, in.pos + text.size, text)
    in.error(text)
)

// URL PARSING
parseType = (in ->
    type = parseManyOne(in, parseNotChars, "./?#=&:; \t\n\r")
    parseChar(type, ":".char).set(type.value.toChar)
)
parseHost = (in ->
    in = parseOptional(in, parseText, "//")
    if not in.value: (
        // TODO this is cheating ... just a bit ...
        colon = in.text.search(":")
        slash = in.text.search("/")
        if not colon: return in
        if slash and slash > colon: return in
    )
    host = parseManyOne(in, parseNotChars, ":/?#;& \t\n\r")
    host.set(host.value.toChar)
)
parsePort = (in ->
    in = parseChar(in, ":".char)
    port = parseMany(in, parseChars, "0123456789")
    port.set(_int_parse(port.value.toChar))
)
parsePath = (in ->
    path = parseMany(in, parseNotChars, "?# \t\n\r")
    text = path.value.toChar
    if text.size == 0: return path.set("/")
    if text.char != "/".char: return path.set("/$(text)")
    path.set(text)
)
parseQuery = (in ->
    in = parseChar(in, "?".char)
    query = parseMany(in, parseNotChars, "# \t\n\r")
    query.set(query.value.toChar)
)
parseFragment = (in ->
    in = parseChar(in, "#".char)
    fragment = parseMany(in, parseNotChars, " \r\n\t")
    fragment.set(fragment.value.toChar)
)
parseUrl = (in ->
    type = parseOptional(in, parseType)
    host = parseOptional(type, parseHost)
    port = parseOptional(host, parsePort)
    path = parsePath(port)
    query = parseOptional(path, parseQuery)
    fragment = parseOptional(query, parseFragment)
    fragment.set({
        type: type.value
        host: host.value
        port: port.value
        path: path.value
        query: query.value
        fragment: fragment.value
    })
)
print parse("foo/foo?bar=;::", parseUrl).path

