if not io.stdin.isTTY: return
term = io.stdin
smcup = true

buf = Buffer.new
if smcup: buf.writeByte 27; buf.write "[?1049h"; term.write buf
term.setRaw

editor = _Object_new {
    _width: 80
    _height: 24

    _lines: Array.new
    _offset: 0
    _x: 1
    _y: 1
    _left: null
    _right: null

    // UTILS
    _debug: Array.new
    debug: l -> this._debug.add(l)

    ensureLeftRight: (->
        assert this._x > 0
        assert this._y > 0
        if this._left: return
        line = this._lines[this._y] or ""

        // when moving along tip of lines, we normalize it here
        if this._x > line.size + 1: this._x = line.size + 1
        this._left = line[:this._x - 1]
        this._right = line[this._x:]
    )
    clearLeftRight: (->
        assert this._x > 0
        assert this._y > 0
        if not this._left: return
        this._lines[this._offset + this._y] = this._left + this._right
        this._left = this._right = null
    )

    // QUERING
    currentline: (->
        if this._left: return this._left + this._right
        this._lines[this._y] or ""
    )

    // EDITING
    // TODO allow input to take "\n" and make lines from that
    input: (s ->
        assert this._x > 0
        assert this._y > 0
        assert not s.find("\n")
        this.ensureLeftRight
        this._left += s
        this._x += s.size
    )
    newline: (->
        this.ensureLeftRight
        this._lines[this._y] = this._left
        this._y += 1
        this._x = 1
        this._lines.insert(this._y, this._right)
        up = this._left

        // figure out indentation of previous line and follow that
        var $indent = 1
        while up[$indent] <= 32: $indent += 1
        this._left = up[:$indent - 1]
        this._x = $indent
        this.debug "line: $(this._y) offset: $(this._offset)"
    )
    deleteleft: (->
        this.ensureLeftRight
        if this._x <= 1:
            if this._y <= 1: return
            line = this._lines[this._y - 1]
            this._lines.remove(this._y)
            this._y -= 1
            this._x = line.size + 1
            assert this._left == ""
            this._left = line
            return

        this._x -= 1
        this._left = this._left[:-2]
    )
    deleteright: (->
        this.ensureLeftRight
        line = this.currentline
        if this._x > line.size:
            if this._y >= this._lines.size: return
            assert this._right == ""
            this._right = this._lines[this._y + 1]
            this._lines.remove(this._y + 1)
            return

        this._right = this._right[2:]
    )

    // MOVING
    moveleft: (->
        if this._x <= 1: return
        this.clearLeftRight
        if this._x > this.currentline.size: this._x = this.currentline.size; return
        this._x -= 1
    )
    moveright: (->
        if this._x >= this._width: return
        this.clearLeftRight
        if this._x >= this.currentline.size + 1: this._x = this.currentline.size + 1; return
        this._x += 1
    )
    movelinestart: (->
        this.clearLeftRight
        this._x = 1
    )
    movelineend: (->
        this.clearLeftRight
        this._x = this.currentline.size + 1
    )
    // TODO take care of offset when moving up/down in lines/pages
    moveup: (->
        if this._y <= 1: return
        this.clearLeftRight
        this._y -= 1
    )
    movedown: (->
        if this._y >= this._lines.size: return
        this.clearLeftRight
        this._y += 1
    )
    movepageup: (->
        this.clearLeftRight
        this._y = max(this._y - this._height, 1)
    )
    movepagedown: (->
        this.clearLeftRight
        this._y = min(this._y + this._height, this._lines.size)
    )

    // RENDER TO vt100 terminal
    // TODO more incremental?? more buffer/view based??
    render: (->
        width, height = term.size
        this._width = width; this._height = height

        buf.writeByte 27; buf.write "[2J"
        height.times: i =>
            y = i + this._offset
            buf.writeByte 27; buf.write "[$i;0H"

            if y == this._y and this._left:
                assert this._right
                buf.write this._left; buf.write this._right
            if not (y == this._y and this._left): buf.write this._lines[y] or ""

        this.debug "$(this._x),$(this._y)"
        if this._debug.size > 0:
            buf.writeByte 27; buf.write "[$(height);0H"
            buf.writeByte 27; buf.write "[2K"
            this._debug.each: l, i ->
                if i != 1: buf.write " "
                buf.write l
            // TODO this._debug.clear
            this._debug = Array.new

        assert this._x > 0
        assert this._y > 0
        x = min(this._x, this.currentline.size + 1)
        buf.writeByte 27; buf.write "[$(this._y);$(x)H"
        term.write buf
    )
}

inbuf = Buffer.new
editor.render
var $running = true
while $running:
    term.read inbuf
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

term.restore
if smcup: buf.writeByte 27; buf.write "[?1049l"; term.write buf

