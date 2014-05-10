// get current cursor position
cursorPosition = term ->
    term.write 27, "[6n"
    len = term.find "R"
    size = term.readString(len)[3:-2].split(";")
    return Number(size[2]), Number(size[1])

term = io.stdin
if not term.isTTY: return
term.setRaw

width, height = term.measure
x, y = cursorPosition(term)

editor = {
    moveleft = -> null
    moveright = -> null
    moveup = -> null
    movedown = -> null
    movelineend = -> null
    movelinestart = -> null
    movepageup = -> null
    movepagedown = -> null

    deleteleft = -> null
    deleteright = -> null

    newline = -> term.write "\n", 27, "[2K", 27, "[1G", "> "
    input = byte -> term.write byte

    debug = str -> null //term.write 27, "[s", 27, "[1;1H", 27, "[2K", str, 27, "[u"
    render = -> null
}

term.write 27, "[2K", "> "

inbuf = Buffer.new
var $running = true
while $running:
    term.readInto inbuf
    while inbuf.size > 0:
        c = inbuf.readByte
        editor.debug "in: $(c)"
        if c == 4: $running = false
        if c == 3: $running = false
        if c == 13: editor.newline
        if c == 127: editor.deleteleft
        if c == 27: // esc
            c2 = inbuf.readByte // [
            c3 = inbuf.readByte // A for up, etc
            editor.debug "esc: $(c2) $(c3)"
            if c2 == 91:
                if c3 == 68: editor.moveleft
                if c3 == 67: editor.moveright
                if c3 == 65: editor.moveup
                if c3 == 66: editor.movedown
                if c3 == 70: editor.movelineend
                if c3 == 72: editor.movelinestart
                // these end with a ~ (always, or maybe they take params?)
                if c3 == 51: inbuf.readByte; editor.deleteright
                if c3 == 53: inbuf.readByte; editor.movepageup
                if c3 == 54: inbuf.readByte; editor.movepagedown
        if c > 31 and c < 127: editor.input(c.toChar)
        // todo more escape sequences and utf8 (when hotel supports that)
    editor.render

