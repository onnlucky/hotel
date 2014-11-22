FONT = { system.platform == "macosx" }: "menlo"
       {                             }: "courier new"
FHEIGHT = 17
FWIDTH = 8
FSIZE = 14

Editor = text, readonly, singleline -> _Mutable_new {
    // to be a "widget"
    class = widget.widget_class
    parent = null
    visible = true
    x = 0
    y = 0
    width = 0
    height = 0

    // editor
    error = null
    errormsg = null

    singleline = singleline or false
    readonly = readonly or false
    cursor = not readonly

    _lines = Array.new((text or "").split("\n"))
    _markup = Array.new()

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

    ensureLeftRight = ->
        assert this._x > 0
        assert this._y > 0
        if this._left: return
        line = this._lines[this._y] or ""

        // when moving along tip of lines, we normalize it here
        if this._x > line.size + 1: this._x = line.size + 1
        this._left = line[:this._x - 1]
        this._right = line[this._x:]

    clearLeftRight = ->
        assert this._x > 0
        assert this._y > 0
        if not this._left: return
        this._lines[this._offset + this._y] = this._left + this._right
        this._left = this._right = null

    addLine = line ->
        if this.singleline: return
        if this._lines[-1] == "":
            this._lines[-1] = line
            return
        this._lines.add(line)

    addLines = lines ->
        if this.singleline: return
        if this._lines[-1] == "":
            this._lines.remove(-1)
        lines.each: l -> this._lines.add(l)

    // QUERING
    currentline = ->
        if this._left: return this._left + this._right
        this._lines[this._y] or ""

    // EDITING
    // TODO allow input to take "\n" and make lines from that
    input = s ->
        assert this._x > 0
        assert this._y > 0
        s = { this.singleline }: s.split("\n")[1]
            {                 }: s

        { this._sx }:
            this.replaceselection(s)
            this._x += s.size
            return
        { s.find("\n") }:
            this.ensureLeftRight
            s = (this._left + s + this._right).split("\n")
            this._lines.remove(this._y)
            s.each: l, n -> this._lines.insert(this._y + n - 1, l)
            this._y = this._y + s.size - 1
            this._x = s[-1].size - this._right.size + 1
            this._left = null
            this._right = null
            return
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

    newline = ->
        if this.singleline: return
        if this._sx: this.replaceselection(""); return

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
        this.debug("line: $(this._y) offset: $(this._offset)") //"

    deleteleft = ->
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

    deleteright = ->
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
        {                     }: this._sx = null; this.sy = null

    replaceselection = text ->
        text = { this.singleline }: text.split("\n")[1]
               {                 }: text
        this.clearLeftRight
        bx, by, ex, ey = this.selection
        if not bx: return
        bline = this._lines[by]
        eline = this._lines[ey]
        (ey - by).times: this._lines.remove(by)
        this._sx = null; this._sy = null
        { text.find("\n") }:
            s = (bline[:bx - 1] + text + eline[ex + 1:]).split("\n")
            this._lines.remove(by)
            s.each: l, n -> this._lines.insert(by + n - 1, l)
            this._y = by + s.size - 1
            this._x = this._lines[this._y].size - eline[ex + 1:].size + 1
        {                 }:
            this._lines[by] = bline[:bx - 1] + text + eline[ex + 1:]
            this._y = by
            this._x = max(bx + text.size - 1, 1)

   selectionText = ->
        this.clearLeftRight
        bx, by, ex, ey = this.selection
        if not bx: return
        if by == ey: return this._lines[by][bx:ex]

        res = Array.new
        res.add(this._lines[by][bx:])
        (ey - by - 1).times: n -> res.add(this._lines[by + n])
        res.add(this._lines[ey][:ex])
        return res.join("\n")

    selectword = ->
        this.clearLeftRight
        this._sy = this._y
        line = this._lines[this._y]
        r = Regex("[1-9a-zA-Z_]+")
        findloop = at ->
            b, e = r.find(line, at)
            if not e: return this._x, this._x
            if b > this._x: return this._x, this._x
            if e >= this._x: return b, e
            goto(findloop(e + 1))
        bx, ex = findloop(1)
        this._x = bx
        this._sx = ex
    selectline = ->
        this.clearLeftRight
        this._sy = this._y
        this._x = 1
        this._sx = this._lines[this._y].size
    selectall = ->
        this.clearLeftRight
        this._x = 1
        this._y = 1
        this._sx = max(this._lines[-1].size, 1)
        this._sy = max(this._lines.size, 1)

   cut = ->
       if not this._sx: return
       this.app.clipboardSet(this.selectionText)
       this.replaceselection("")

    copy = ->
       if not this._sx: return
       this.app.clipboardSet(this.selectionText)

    paste = ->
        if this._sx: this.replaceselection(this.app.clipboardGet()); return
        this.input(this.app.clipboardGet())

    // TODO implement undo of sorts ...
    undo = -> null

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
    size = g, bwidth, bheight ->
        this.clearLeftRight
        width = this._lines.map(l, n ->
            { n == this._y and this._left }: this._left.size + this._right.size
            {                             }: l.size
        ).max + 1
        height = this._lines.size
        height = { readonly and height > 1 and this._lines[-1] == "" }: height - 1
                 {                                                   }: height
        this.width = max(width * FWIDTH, bwidth)
        this.height = height * FHEIGHT

    lines = ->
        lines = this._lines.size
        if lines == 1 and this._lines[1].size == 0: return 0
        return lines

    text = ->
        this.clearLeftRight
        this._lines.join("\n")

    setText = text ->
        ls = { this.singleline }: [text.split("\n")[1]]
             {                 }: text.split("\n")
        this._lines = Array.new(ls)
        this._y = 1
        this._x = 1
        this._sx = null
        this._sy = null
        this._left = null

    clearaftercaret = ->
        this._sx = null
        this._sy = null
        this.ensureLeftRight
        res = this._right +"\n"+ this._lines[this._y + 1:].join("\n")
        this._right = ""
        while this._lines.size > this._y:
            this._lines.remove(this._y + 1)
        return res

    clear = ->
        this.setText("")

    draw = g, width, height ->
        g.setFont(FONT, FSIZE)

        if not this.readonly:
            g.color(1,1,1).rectangle(0, 0, width, height).fill
        if this.error:
            g.color(1,0.9,0.9).rectangle(0, this.error * FHEIGHT - FHEIGHT, width, FHEIGHT).fill
        if this._sx:
            { this.focus }: g.color(0.7,0.83,1)
            {            }: g.color(0.7,0.7,0.7)
            bx, by, ex, ey = this.selection
            { by == ey }:
                g.rectangle(bx * FWIDTH - FWIDTH, by * FHEIGHT - FHEIGHT, (ex - bx + 1) * FWIDTH, FHEIGHT)
            {          }:
                g.rectangle(bx * FWIDTH - FWIDTH, by * FHEIGHT - FHEIGHT, width, FHEIGHT)
                g.rectangle(0, by * FHEIGHT, 1000, (ey - by - 1) * FHEIGHT)
                g.rectangle(0, ey * FHEIGHT - FHEIGHT, ex * FWIDTH, FHEIGHT)
            g.fill
        if this.cursor:
            g.color(0.5,0.5,0.5)
            x = { this._left           }: min(this._left.size + this._right.size + 1, this._x)
                { this._lines[this._y] }: min(this._lines[this._y].size + 1, this._x)
                {                      }: this._x
            g.rectangle(x * FWIDTH - FWIDTH, this._y * FHEIGHT - FHEIGHT, FWIDTH, FHEIGHT)
            { this.focus }: g.fill
            {            }: g.stroke

        this._lines.size.times: r ->
            y = r * FHEIGHT - 3
            if y > height: return
            line = { this._y == r and this._left }: this._left + this._right
                   {                             }: this._lines[r]
            if not line: break

            g.color
            markup = this._markup[r] or []
            line.size.times: c ->
                x = c * FWIDTH - FWIDTH
                if x > width: return
                if markup[c]: g.color(markup[c])
                g.moveTo(x, y).fillText(line[c])

    onmouse = event, mx, my ->
        if event.buttons == 0: return this.app.onmousemove(null)
        this.setFocus()
        this._sx = null
        this._sy = null
        x = max(mx /. FWIDTH + 1, 1)
        y = max(my /. FHEIGHT + 1, 1)
        if y > this._lines.size:
            this._x = this._lines[-1].size + 1
            this._y = this._lines.size
            return
        this._y = y
        this._x = min(x, this._lines[y].size + 1)

        if event.clicks == 2: this.selectword
        if event.clicks == 3: this.selectline
        this.parent.dirty

        this.app.onmousemove(mx, my, buttons ->
            x = max((mx - this.x) /. FWIDTH + 1, 1)
            y = max((my - this.y) /. FHEIGHT + 1, 1)
            if y > this._lines.size:
                this._sx = this._lines[-1].size + 1
                this._sy = this._lines.size
                return
            this._sy = y
            this._sx = min(x, this._lines[y].size + 1)
            this.parent.dirty
        )

    onkey = event ->
        c = event.key
        shift = event.modifiers & 1 == 1
        ctrl = event.modifiers & 2 == 2

        if this.readonly:
            { ctrl and c == 'X' }: this.copy
            { ctrl and c == 'C' }: this.copy
            {                   }: null
            return

        var change = true
        { ctrl and c == 'A' }: this.selectall; change = false
        { ctrl and c == 'C' }: this.copy
        { ctrl and c == 'X' }: this.cut
        { ctrl and c == 'V' }: this.paste
        { ctrl and c == 'Z' }: this.undo
        { ctrl and c == 37  }: this.movelinestart(shift); change = false
        { ctrl and c == 39  }: this.movelineend(shift); change = false
        { c ==  13 and this.singleline }: change = false; if (this.onsubmit): this.onsubmit()
        { c ==  13 }: this.newline
        { c ==   8 }: this.deleteleft
        { c ==   9 }: this.input("  ")
        { c == 127 }: this.deleteright
        { c ==  37 }: this.moveleft(shift); change = false
        { c ==  39 }: this.moveright(shift); change = false
        { c ==  38 }: this.moveup(shift); change = false
        { c ==  40 }: this.movedown(shift); change = false
        { c ==  36 }: this.movelinestart(shift); change = false
        { c ==  35 }: this.movelineend(shift); change = false
        { c ==  33 }: this.movepageup(shift); change = false
        { c ==  34 }: this.movepagedown(shift); change = false
        { event.input.size == 1 }: this.input(event.input)
        {          }: print "unknown key", repr(event); change = false
        this.parent.dirty
        if change and this.onchange: this.onchange()

    onchange = null
    onsubmit = null

    setErrorLine = line, msg ->
        this.error = line
        this.errormsg = msg
}

