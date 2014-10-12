_env = Env.current
var $env = Env.new(_env)

// get current cursor position
cursorPosition = term ->
    term.write(27, "[6n")
    len = term.find("R")
    size = term.readString(len)[3:-2].split(";")
    return Number(size[2]), Number(size[1])

// TODO listen for signal WINCH

term = io.stdin
if not term.isTTY: return
term.setRaw

var $last = {cursor=0,lines=0}
writeLine = prompt, left, right ->
    width, height = term.measure
    len = prompt.size + left.size + right.size
    lines = (len / width).floor
    cursor = ((prompt.size + left.size) / width).floor
    curcol = prompt.size + left.size + 1 - width * cursor

    // ensure we are at first line
    buf = Buffer.new
    trail = $last.lines - lines
    up = $last.cursor
    if up > 0: buf.write(27, "[$(up)A")
    // output line
    buf.write(27, "[0G", prompt, left, right, 27, "[0K")
    // clear trailing lines from previous long entry
    if trail > 0:
        buf.write(27, "[0G")
        trail.times: buf.write(27, "[1B", 27, "[0K")
        trail.times: buf.write(27, "[1A")
    // move cursor back up to cursor position line and col
    if lines - cursor > 0: buf.write(27, "[$(lines - cursor)A")
    buf.write(27, "[$(curcol)G")
    $last = {{cursor,lines}}
    term.write(buf)

runcode = code ->
    term.restore
    print ""
    (
        catch: e -> print "error: $(e.toString)"
        res = eval(env, code)
        if res:
            r = repr(res)
            { r == "null"   }: print res
            {               }: print r
    )
    term.write(" ") // seems needed to flush print's and such
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
    older.add(line)
    history = older.toList[-100:]
    content = history.join("\n") + "\n"
    io.File(HIST).write(content)

prompt = "> "
var $left = ""
var $right = ""
editor = {
    setLine = line ->
        $left = line
        $right = ""
        writeLine prompt, $left, $right
    moveup = ->
        if newer.size == 0: readSharedHistory()
        if older.size == 0: return
        newer.add($left + $right)
        this.setLine(older.remove(-1), $left.size)
    movedown = ->
        if newer.size == 0: return
        older.add($left + $right)
        this.setLine(newer.remove(-1), $left.size)
    movepageup = -> null
    movepagedown = -> null

    moveleft = ->
        if $left.size == 0: return
        $right = $left[-1:] + $right
        $left = $left[:-2]
        writeLine prompt, $left, $right
    moveright = ->
        if $right.size == 0: return
        $left = $left + $right[:1]
        $right = $right[2:]
        writeLine prompt, $left, $right
    movelineend = ->
        $left = $left + $right
        $right = ""
        writeLine prompt, $left, $right
    movelinestart = ->
        $right = $left + $right
        $left = ""
        writeLine prompt, $left, $right
    deleteleft = ->
        if $left.size == 0: return
        $left = $left[:-2]
        writeLine prompt, $left, $right
    deleteright = ->
        if $right.size == 0: return
        $right = $right[2:]
        writeLine prompt, $left, $right
    input = str ->
        $left += str
        writeLine prompt, $left, $right
    special = byte ->
        { byte == 18 }: this.debug("RUN!")
        {            }: this.debug("$byte")

    debug = str -> null //term.write 27, "[s", 27, "[1;1H", 27, "[2K", str, 27, "[u"
    newline = ->
        line = $left + $right
        addHistory(line)
        runcode(line)
        this.setLine("")
}

writeLine prompt, $left, $right

commandline = _env ->
    if _env: $env = _env

    inbuf = Buffer.new
    var $running = true
    while $running:
        term.readInto(inbuf)
        while inbuf.size > 0:
            c = inbuf.readByte
            editor.debug("in: $(c)")
            { c == 4   }: $running = false
            { c == 3   }: $running = false
            { c == 13  }: editor.newline
            { c == 127 }: editor.deleteleft
            { c == 27  }: // esc
                c2 = inbuf.readByte // [
                c3 = inbuf.readByte // A for up, etc
                editor.debug("esc: $(c) $(c2) $(c3)")
                if not c2: editor.special(c)
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
    term.write("\n")

