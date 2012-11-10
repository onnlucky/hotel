// http request and url parser
DEBUG=false
trace = { in, &msg -> if DEBUG: print in.pos, msg() }

lower = "abcdefghijklmnopqrstuvwxyz"
upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
numbers = "0123456789"

// TODO should be a class/object but class needs constructor and constructor needs class ...
input = { text, pos, value -> {
        text: text
        pos: pos
        value: value
        item: text.get(pos)
        end: -> this.pos >= this.text.size
        set: val -> input(this.text, this.pos, val)
        do: block -> this.set(block(this.value))
        next: val -> input(this.text, this.pos + 1, val)
        error: val -> _throw this.set(val)
    }
}
rule = fn -> { a1, a2, a3, a4 -> {in -> fn(in, a1, a2, a3, a4)}}

parse = { parser, text ->
    out = (
        _catch: e -> _throw "syntax error"
        parser(input(text, 0, null))
    )
    if not out.end: _throw "expected end of input"
    return out.value
}

pOptional = rule({in, parser ->
    trace(in, "pOptional")
    _catch: e -> trace(in, "ERROR: $e"); return in.set(null)
    in = parser(in)
})
_pmany = {in, parser, res ->
    trace(in, "_pmany")
    _catch: e -> trace(in, "ERROR: $e"); return in.set(res)
    in = parser(in)
    goto _pmany(in, parser, res.add(in.value))
}
pMany = rule({in, parser ->
    trace(in, "pMany")
    _pmany(in, parser, [])
})
pManyOne = rule({in, parser ->
    trace(in, "pManyOne")
    in = parser(in)
    _pmany(in, parser, [in.value])
})
pChar = rule({in, ch ->
    trace(in, "pChar: $(in.item.toChar) == $(ch.toChar)")
    if in.item == ch: return in.next(ch)
    in.error(ch.toChar)
})
pChars = rule({in, chars ->
    trace(in, "pChars: $(in.item.toChar) in $(chars)")
    if chars.find(in.item.toChar): return in.next(in.item)
    in.error(chars)
})
pNotChars = rule({in, chars ->
    trace(in, "pNotChars: $(in.item.toChar) not in $(chars)")
    if chars.find(in.item.toChar): return in.error("not $chars")
    in.next(in.item)
})
pText = rule({in, text ->
    trace(in, "pText: $(text)")
    if in.text.startsWith(text, in.pos): return input(in.text, in.pos + text.size, text)
    in.error(text)
})

// URL PARSING
parseType = {in ->
    type = pManyOne(pNotChars(":/?# \n\r"))(in).do: v -> v.toChar
    pChar(":".char)(type).set(type.value)
}
parseHost = {in ->
    in = pOptional(pText("//"))(in)
    if not in.value: {
        // TODO this is cheating ... just a bit ...
        colon = in.text.find(":", in.pos)
        slash = in.text.find("/", in.pos)
        if not colon: return in
        if slash and slash > colon: return in
    }
    host = pManyOne(pNotChars(":/?#;& \t\n\r"))(in).do: v -> v.toChar
}
parsePort = {in ->
    in = pChar(":".char)(in)
    port = pMany(pChars(numbers))(in).do: v -> v.toChar.eval
}
parsePath = {in ->
    path = pMany(pNotChars("?# \t\n\r"))(in).do: v -> v.toChar
    text = path.value
    if text.size == 0: return path.set("/")
    if text.char != "/".char: return path.set("/$(text)")
    return path
}
parseRequestPath = {in ->
    path = pMany(pNotChars("# \t\n\r"))(in).do: v -> v.toChar
    text = path.value
    if text.size == 0: return path.set("/")
    if text.char != "/".char: return path.set("/$(text)")
    return path
}
parseQuery = {in ->
    in = pChar("?".char)(in)
    query = pMany(pNotChars("# \t\n\r"))(in).do: v -> v.toChar
}
parseFragment = {in ->
    in = pChar("#".char)(in)
    fragment = pMany(pNotChars(" \r\n\t"))(in).do: v -> v.toChar
}
parseUrl = {in ->
    trace(in, "parseUrl")
    type = pOptional(parseType)(in)
    trace(type, "TYPE: $(type.value)")
    host = pOptional(parseHost)(type)
    port = pOptional(parsePort)(host)
    path = parsePath(port)
    reqpath = parseRequestPath(port) // not a mistake!
    query = pOptional(parseQuery)(path)
    fragment = pOptional(parseFragment)(query)
    fragment.set({
        type: type.value
        host: host.value
        port: port.value
        request: reqpath.value
        fragment: fragment.value
        path: path.value
        query: query.value
    })
}
parseSpaces = {in ->
    pManyOne(pChars("\t "))(in)
}
parseNewline = {in ->
    trace(in, "parseNewline")
    try: return pText("\r\n")(in)
    try: return pText("\n\r")(in)
    try: return pText("\n")(in)
    pText("\r")(in)
}
parseHeader = {in ->
    trace(in, "parseHeader")
    key = pManyOne(pNotChars(":\r\n"))(in).do: v -> v.toChar
    colon = pChar(":".char)(key)
    sp = pOptional(parseSpaces)(colon)
    value = pManyOne(pNotChars("\r\n"))(sp).do: v -> v.toChar
    nl = parseNewline(value)
    trace(nl, "KEY: $(key.value) VALUE: $(value.value)")
    nl.set({
        key: key.value
        value: value.value
    })
}
createmap = { ls ->
    map = HashMap.new
    ls.each: { e -> map.set e.key, e.value }
    return map
}
parseHttpRequest = {in ->
    method = pManyOne(pChars(upper))(in).do: v -> v.toChar
    trace(method, "METHOD: $(method.value)")
    sp = parseSpaces(method)
    url = parseUrl(sp)
    sp = parseSpaces(url)
    http = pText("HTTP/1.")(sp)
    trace(http, "HTTP: $(http.value)")
    version = pChars(numbers)(http).do: v -> "1.$(v.toChar)"
    nl = parseNewline(version)
    headers = pMany(parseHeader)(nl)
    nl = parseNewline(headers)
    nl.set({
        method: method.value
        url: url.value
        version: version.value
        headers: createmap(headers.value)
    })
}
parseHttpResponse = {in ->
    http = pMany(pNotChars(" \r\n"))(in)
    sp = pOptional(parseSpaces)(http)
    status = pMany(pChars(numbers))(sp).do: v -> v.toChar.eval
    line = pMany(pNotChars("\r\n"))(status)
    nl = parseNewline(line)
    headers = pMany(parseHeader)(nl)
    nl = parseNewline(headers)
    nl.set({
        status: status.value
        headers: createmap(headers.value)
    })
}

{
    request: {text -> parse(parseHttpRequest, text) }
    response: { text -> parse(parseHttpResponse, text) }
    url: { text -> parse(parseUrl, text) }
}

