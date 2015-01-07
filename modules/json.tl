parse = s ->
    { isBuffer(s) }: return _json_parse(s.readString())
    {             }: return _json_parse(s.toString)

// TODO add pretty printing and such ...
into_buf = buf, x, d, set ->
    if d > 1000: Error("json is circular or too deep").throw
    { isBool(x)   }: buf.write(x.toString)
    { not x       }: buf.write("null")
    { isNumber(x) }: buf.write(x.toString)
    { isString(x) }: buf.write("\""); buf.write(x.escape); buf.write("\"")
    { isList(x) or isArray(x) }:
        if isArray(x) and not set.add(x): buf.write("null"); return
        buf.write("[")
        x.each: v, n ->
            if n > 1: buf.write(",")
            into_buf(buf, v, d + 1, set)
        buf.write("]")
    { isObject(x) }:
        buf.write("{")
        Object.each(x): k, v, n ->
            if n > 1: buf.write(",")
            buf.write("\"", k.escape, "\"", ":")
            into_buf(buf, v, d + 1, set)
        buf.write("}")
    { isMap(x) or isHashMap(x) }:
        if isHashMap(x) and not set.add(x): buf.write("null"); return
        buf.write("{")
        x.each: k, v, n ->
            if n > 1: buf.write(",")
            buf.write("\"", k.escape, "\"", ":")
            into_buf(buf, v, d + 1, set)
        buf.write("}")
    { isSet(x)    }:
        buf.write("{")
        x.each: k, n ->
            if n > 1: buf.write(",")
            buf.write("\"", k.escape, "\"", ":true")
        buf.write("}")
    {             }: buf.write("null")

toBuffer = o ->
    buf = Buffer.new
    into_buf(buf, o, 0, IdentitySet.new)
    return buf
toBin = o -> toBuffer(o).read
toString = o -> toBuffer(o).readString

{{ parse, toBuffer, toBin, toString }}
