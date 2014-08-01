_msg = Array.new
env = (
    print = -> _msg.addAll(args.join(" ").split("\n",keepempty=true)); null
    _env = Env.current
    Var.new(Env.new(_env))
)

w = Window.new 400, 400
w.title "tl scratchpad"
b = Box.new 0, 0, w.width, w.height
w.add(b)
w.onresize(width, height -> b.width(width); b.height(height))
!(sleep 0.1; w.focus)

FHEIGHT = 17
FWIDTH = 8
FSIZE = 14

Editor = readonly -> _Mutable_new {
    error = false
    cursor = not readonly

    _lines = Array.new([""])
    _offset = 0
    _x = 1
    _y = 1
    _left = null
    _right = null
    _sx = null
    _sy = null

    // UTILS
    _debug = Array.new
    debug = l -> this._debug.add(l)

    ensureLeftRight = (->
        assert this._x > 0
        assert this._y > 0
        if this._left: return
        line = this._lines[this._y] or ""

        // when moving along tip of lines, we normalize it here
        if this._x > line.size + 1: this._x = line.size + 1
        this._left = line[:this._x - 1]
        this._right = line[this._x:]
    )
    clearLeftRight = (->
        assert this._x > 0
        assert this._y > 0
        if not this._left: return
        this._lines[this._offset + this._y] = this._left + this._right
        this._left = this._right = null
    )

    // QUERING
    currentline = (->
        if this._left: return this._left + this._right
        this._lines[this._y] or ""
    )

    // EDITING
    // TODO allow input to take "\n" and make lines from that
    input = (s ->
        assert this._x > 0
        assert this._y > 0
        assert not s.find("\n")

        { this._sx }:
            this.replaceselection(s)
            this._x += s.size
            this.ensureLeftRight
        {          }:
            this.ensureLeftRight
            this._left += s
            this._x += s.size

        null // TODO needed due to syntax "bug", two truth tables, but recognized as one ...

        // post processing ...
        { s == "("                             }: this._right = ")"+ this._right
        { s == ")" and this._right[1] == ')'   }: this._right = this._right[2:]
        { s == "\"" and this._right[1] == '"'  }: this._right = this._right[2:]
        { s == "\""                            }: this._right = "\""+ this._right
        { s == "'" and this._right[1:1] == "'" }: this._right = this._right[2:]
        { s == "'"                             }: this._right = "'"+ this._right
        {                                      }: null
    )

    newline = (->
        if this._sx: this.replaceselection("")

        this.ensureLeftRight
        this._lines[this._y] = this._left
        this._y += 1
        this._x = 1
        this._lines.insert(this._y, this._right)
        up = this._left

        // figure out indentation of previous line and follow that
        var $indent = 1
        while $indent <= up.size and up[$indent] <= 32: $indent += 1
        this._left = up[:$indent - 1]
        this._x = $indent
        this.debug "line: $(this._y) offset: $(this._offset)"
    )
    deleteleft = (->
        if this._sx: this.replaceselection(""); return
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
    deleteright = (->
        if this._sx: this.replaceselection(""); return
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

    // SELECTION
    selection = ->
        if not this._sx: return null
        by, ey = { this._sy < this._y }: multi(this._sy, this._y)
                 {                    }: multi(this._y, this._sy)
        bx, ex = { this._sx < this._x }: multi(this._sx, this._x)
                 {                    }: multi(this._x, this._sx)
        return bx, by, ex, ey

    // TODO this is incorrect ... but almost
    setselection = select ->
        { select and this._sx }: null
        { select              }: this._sx = this._x; this._sy = this._y
        {                     }: this._sx = null

    selectall = ->
        this.clearLeftRight
        this._x = 1
        this._y = 1
        this._sx = this._lines[-1].size
        this._sy = this._lines.size

    replaceselection = text ->
        this.clearLeftRight
        bx, by, ex, ey = this.selection
        if not bx: return
        bline = this._lines[by]
        eline = this._lines[ey]
        (ey - by).times: this._lines.remove(by)
        this._lines[by] = bline[:bx - 1] + text + eline[ex + 1:]
        this._sx = null; this._sy = null
        this._x = bx; this._y = by

    // MOVING
    moveleft = select ->
        this.setselection(select)
        if this._x <= 1: return
        this.clearLeftRight
        if this._x > this.currentline.size: this._x = this.currentline.size; return
        this._x -= 1
    moveright = select ->
        this.setselection(select)
        this.clearLeftRight
        if this._x >= this.currentline.size + 1: this._x = this.currentline.size + 1; return
        this._x += 1
    movelinestart = select ->
        this.setselection(select)
        this.clearLeftRight
        this._x = 1
    movelineend = select ->
        this.setselection(select)
        this.clearLeftRight
        this._x = this.currentline.size + 1
    moveup = select ->
        this.setselection(select)
        if this._y <= 1: return
        this.clearLeftRight
        this._y -= 1
    movedown = select ->
        this.setselection(select)
        if this._y >= this._lines.size: return
        this.clearLeftRight
        this._y += 1
    movepageup = select ->
        this.setselection(select)
        this.clearLeftRight
        this._y = max(this._y - 20, 1)
    movepagedown = select ->
        this.setselection(select)
        this.clearLeftRight
        this._y = min(this._y + 20, this._lines.size)

    // QUERYING
    size = (->
        width = this._lines.map(l, n ->
            { n == this._y and this._left }: this._left.size + this._right.size
            {                             }: l.size
        ).max
        return width * FWIDTH, this._lines.size * FHEIGHT
    )

    setText = text ->
        this._lines = Array.new(text.split("\n",keepempty=true))
        this._y = 1
        this._x = 1
        this._left = null

    text = ->
        this.clearLeftRight
        this._lines.join("\n")

    clear = -> this.setText("")

    render = (g ->
        g.setFont("menlo", FSIZE)

        if this._sx:
            g.color(0.7,0.83,1)
            bx, by, ex, ey = this.selection
            { by == ey }:
                g.rectangle(bx * FWIDTH - FWIDTH, by * FHEIGHT - FHEIGHT, (ex - bx + 1) * FWIDTH, FHEIGHT)
            {          }:
                g.rectangle(bx * FWIDTH - FWIDTH, by * FHEIGHT - FHEIGHT, 1000, FHEIGHT)
                g.rectangle(0, by * FHEIGHT, 1000, (ey - by - 1) * FHEIGHT)
                g.rectangle(0, ey * FHEIGHT - FHEIGHT, ex * FWIDTH, FHEIGHT)
            g.fill
        if this.cursor:
            g.color(0.5,0.5,0.5)
            x = { this._left           }: min(this._left.size + this._right.size + 1, this._x)
                { this._lines[this._y] }: min(this._lines[this._y].size + 1, this._x)
                {                      }: this._x
            g.rectangle(x * FWIDTH - FWIDTH, this._y * FHEIGHT - FHEIGHT, FWIDTH, FHEIGHT)
            g.fill

        g.color(0,0,0)
        this._lines.size.times: r ->
            line = { this._y == r and this._left }: this._left + this._right
                   {                             }: this._lines[r]
            if not line: break
            line.size.times: c ->
                g.moveTo(c * FWIDTH - FWIDTH, r * FHEIGHT - 3)
                g.fillText(line[c:c])
    )
}

editor = Editor()
output = Editor(true)

runcode = code ->
    _msg.clear
    catch: e ->
        output.error = true
        output.setText("error: $(e.toString)") //"
    res = eval(env, code)
    if res:
        r = repr(res)
        { r == "null"   }: _msg.add(res)
        {               }: _msg.add("$r")
    output.error = false
    output.setText(_msg.join("\n"))

w.onkey(event ->
    catch: e -> print e.toString
    print repr event
    c = event.key
    shift = event.modifiers & 1 == 1
    ctrl = event.modifiers & 2 == 2
    { ctrl and c == 'Q' }: w.close
    { ctrl and c == 'A' }: editor.selectall
    { c ==  27 }: runcode(editor.text)
    { c ==  13 }: editor.newline
    { c ==   8 }: editor.deleteleft
    { c == 127 }: editor.deleteright
    { c ==  37 }: editor.moveleft(shift)
    { c ==  39 }: editor.moveright(shift)
    { c ==  38 }: editor.moveup(shift)
    { c ==  40 }: editor.movedown(shift)
    { c ==  36 }: editor.movelinestart(shift)
    { c ==  35 }: editor.movelineend(shift)
    { c ==  33 }: editor.movepageup(shift)
    { c ==  34 }: editor.movepagedown(shift)
    { event.input.size == 1 }: editor.input(event.input)
    {          }: print "unhandled key"
    b.redraw
)

w.onmouse(x, y ->
    print x, y
)

b.ondraw(g ->
    catch: e -> print e.toString
    width, height = editor.size
    g.save
    g.translate(10, 10)
    g.rectangle(0, 0, width, height)
    g.color 1,1,1
    g.fill
    editor.render(g)
    g.restore

    g.translate(10, 10 + 10 + height)
    width, height = output.size
    { output.error }: g.color 1,0.8,0.8
    {              }: g.color 1,1,1
    g.rectangle(0, 0, width, height)
    g.fill
    output.render(g)
    g.restore
)

editor.setText("print \"hello world!\"")

