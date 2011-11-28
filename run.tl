if = cond, block -> bool(cond, block, (->))()
while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

file = _File_open("Makefile", _File_RDONLY)
buf = _Buffer_new()
file.read buf
input = buf.read

var $lines = []
addline = (line, indent, start, end ->
    $lines = $lines.add({line: line, indent: indent, text: input.slice(start, end)})
)

parse = (line, at ->
    if at >= input.size: return

    spaces = (at, indent ->
        if input.startsWith(" ", at): goto(spaces(at + 1, indent + 1))
        if input.startsWith("\t", at): goto(spaces(at + 1, indent + 4))
        return at, indent
    )
    start, indent = spaces(at, 0)

    newlines = (at ->
        if input.size == at: return at, at
        if input.startsWith("\r\n", at): return at, at + 2
        if input.startsWith("\n\r", at): return at, at + 2
        if input.startsWith("\n", at): return at, at + 1
        if input.startsWith("\r", at): return at, at + 1
        goto(newlines(at + 1))
    )
    end, new = newlines(start)
    addline(line, indent, start, end)
    goto(parse(line + 1, new))
)
parse(1, 0)

var $at = 0
while $at < $lines.size: (
    line = $lines.get($at)
    print line.line, line.indent, "'$(line.text)'"
    $at += 1
)

