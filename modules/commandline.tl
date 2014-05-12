_env = Env.current
var $env = Env.new(_env)

// get current cursor position
cursorPosition = term ->
    term.write 27, "[6n"
    len = term.find "R"
    size = term.readString(len)[3:-2].split(";")
    return Number(size[2]), Number(size[1])

term = io.stdin
if not term.isTTY: return
term.setRaw

runcode = code ->
    term.restore
    print ""
    (
        catch: e -> print "error: $(e.toString)"
        res = eval(env, code)
        if res: print (repr(res) or r)
    )
    term.write " " // seems needed to flush print's and such
    term.setRaw

//width, height = term.measure
//x, y = cursorPosition(term)

older = Array.new
newer = Array.new
HIST = _io_getenv("HOME") + "/.tlhistory"
readSharedHistory = ->
    history = try(io.File(HIST).readString.split("\n"))
    if not history: return
    older.clear
    older.cat(history)
addHistory = line ->
    readSharedHistory()
    newer.clear
    older.add line
    history = older.toList[-100:]
    content = history.join("\n") + "\n"
    io.File(HIST).write(content)

left = Var.new ""
right = Var.new ""
editor = {
    setLine = line ->
        $left = line
        $right = ""
        term.write 27, "[1G> ", 27, "[K", line
    moveup = ->
        if newer.size == 0: readSharedHistory()
        if older.size == 0: return
        newer.add $left + $right
        this.setLine(older.remove(-1), $left.size)
    movedown = ->
        if newer.size == 0: return
        older.add $left + $right
        this.setLine(newer.remove(-1), $left.size)
    movepageup = -> null
    movepagedown = -> null

    moveleft = ->
        if $left.size == 0: return
        $right = $left[-1:] + $right
        $left = $left[:-2]
        term.write 27, "[", 68
    moveright = ->
        if $right.size == 0: return
        $left = $left + $right[:1]
        $right = $right[2:]
        term.write 27, "[", 67
    movelineend = ->
        $left = $left + $right
        $right = ""
        term.write 27, "[$(2 + $left.size + 1)G"
    movelinestart = ->
        $right = $left + $right
        $left = ""
        term.write 27, "[2G"
    deleteleft = ->
        if $left.size == 0: return
        $left = $left[:-2]
        term.write 27, "[", 68, 27, "[K", $right, 27, "[$(2 + $left.size + 1)G"
    deleteright = ->
        if $right.size == 0: return
        $right = $right[2:]
        term.write 27, "[K", $right, 27, "[$(2 + $left.size + 1)G"
    input = str ->
        $left += str
        { $right.size == 0 }: term.write str
        {                  }: term.write str, 27, "[K", $right, 27, "[$(2 + $left.size + 1)G"
    special = byte ->
        { byte == 18 }: this.debug("RUN RUN RUN forest!")

    debug = str -> null //term.write 27, "[s", 27, "[1;1H", 27, "[2K", str, 27, "[u"
    newline = ->
        line = $left + $right
        addHistory(line)
        runcode(line)
        this.setLine("")
}

term.write 27, "[2K", "> "

inbuf = Buffer.new
var $running = true
while $running:
    term.readInto inbuf
    while inbuf.size > 0:
        c = inbuf.readByte
        editor.debug "in: $(c)"
        { c == 4   }: $running = false
        { c == 3   }: $running = false
        { c == 13  }: editor.newline
        { c == 127 }: editor.deleteleft
        { c == 27  }: // esc
            c2 = inbuf.readByte // [
            c3 = inbuf.readByte // A for up, etc
            editor.debug "esc: $(c) $(c2) $(c3)"
            if not c2: editor.special c
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
        { c > 31 and c < 127 }: editor.input(c.toChar)
        {                    }: editor.special(c)

term.restore
term.write "\n"

