_env = Env.current
FONT = "menlo"
FONT = "Courier New"

w = Window.new 400, 800
w.title "tl scratchpad"
b = Box.new 0, 0, w.width, w.height
w.add(b)
w.onresize(width, height -> b.width(width); b.height(height))
!(sleep 0.1; w.focus)

FHEIGHT = 17
FWIDTH = 8
FSIZE = 14

Editor = text, readonly -> _Mutable_new {
    error = false
    cursor = not readonly
    x = 0
    y = 0

    _lines = Array.new((text or "").split("\n",keepempty=true))
    _paste = ""
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

        { this._sx }:
            this.replaceselection(s)
            this._x += s.size
            return
        { s.find("\n") }:
            this.ensureLeftRight
            s = (this._left + s + this._right).split("\n", keepempty=true)
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
    )

    newline = (->
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
        {                     }: this._sx = null; this.sy = null

    replaceselection = text ->
        this.clearLeftRight
        bx, by, ex, ey = this.selection
        if not bx: return
        bline = this._lines[by]
        eline = this._lines[ey]
        (ey - by).times: this._lines.remove(by)
        this._sx = null; this._sy = null
        { text.find("\n") }:
            s = (bline[:bx - 1] + text + eline[ex + 1:]).split("\n", keepempty=true)
            this._lines.remove(by)
            s.each: l, n -> this._lines.insert(by + n - 1, l)
            this._y = by + s.size - 1
            this._x = this._lines[this._y].size - eline[ex + 1:].size + 1
        {                 }:
            this._lines[by] = bline[:bx - 1] + text + eline[ex + 1:]
            this._y = by
            this._x = bx + text.size

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
        bx = line.find(' ', from=this._x, backward=true) or 0
        ex = line.find(' ', from=this._x) or line.size + 1
        this._x = bx + 1
        this._sx = ex - 1
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
       this._paste = this.selectionText
       this.replaceselection("")

    copy = ->
       if not this._sx: return
       this._paste = this.selectionText

    paste = ->
        if this._sx: this.replaceselection(this._paste); return
        this.input(this._paste)

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
    size = (->
        this.clearLeftRight
        width = this._lines.map(l, n ->
            { n == this._y and this._left }: this._left.size + this._right.size
            {                             }: l.size
        ).max
        height = this._lines.size
        height = { readonly and height > 1 and this._lines[-1] == "" }: height - 1
                 {                                                   }: height
        return width * FWIDTH, height * FHEIGHT
    )

    setText = text ->
        this.clearLeftRight
        this._lines = Array.new(text.split("\n",keepempty=true))
        this._y = 1
        this._x = 1
        this._left = null

    text = ->
        this.clearLeftRight
        this._lines.join("\n")

    clear = -> this.setText("")

    render = (g ->
        g.setFont(FONT, FSIZE)

        if this.cursor and this._sx:
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
        if readonly: g.color(0.4,0.4,0.4)
        this._lines.size.times: r ->
            line = { this._y == r and this._left }: this._left + this._right
                   {                             }: this._lines[r]
            if not line: break
            line.size.times: c ->
                g.moveTo(c * FWIDTH - FWIDTH, r * FHEIGHT - 3)
                g.fillText(line[c:c])
    )

    onmouse = mx, my, buttons, clicks ->
        if buttons == 0: return w.onmousemove(null)
        //print "onmouse:", mx, my, buttons, clicks
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

        if clicks == 2: this.selectword
        if clicks == 3: this.selectline

        w.onmousemove(mx, my, buttons ->
            //print "onmousemove:", mx, my, buttons
            x = max((mx - this.x) /. FWIDTH + 1, 1)
            y = max((my - this.y) /. FHEIGHT + 1, 1)
            if y > this._lines.size:
                this._sx = this._lines[-1].size + 1
                this._sy = this._lines.size
                return
            this._sy = y
            this._sx = min(x, this._lines[y].size + 1)
            b.redraw
        )

    onkey = event ->
        c = event.key
        shift = event.modifiers & 1 == 1
        ctrl = event.modifiers & 2 == 2
        { ctrl and c == 'A' }: this.selectall
        { ctrl and c == 'X' }: this.cut
        { ctrl and c == 'C' }: this.copy
        { ctrl and c == 'V' }: this.paste
        { ctrl and c == 'Z' }: this.undo
        { c ==  13 }: this.newline
        { c ==   8 }: this.deleteleft
        { c ==   9 }: this.input("  ")
        { c == 127 }: this.deleteright
        { c ==  37 }: this.moveleft(shift)
        { c ==  39 }: this.moveright(shift)
        { c ==  38 }: this.moveup(shift)
        { c ==  40 }: this.movedown(shift)
        { c ==  36 }: this.movelinestart(shift)
        { c ==  35 }: this.movelineend(shift)
        { c ==  33 }: this.movepageup(shift)
        { c ==  34 }: this.movepagedown(shift)
        { event.input.size == 1 }: this.input(event.input)
        {          }: print "unknown key", repr(event)
}

Snippet = content -> _Mutable_new {
    inputenv = null
    changed = true
    code = null
    env = Var.new
    editor = Editor(content)
    output = Editor(readonly=true)
    x = 0, y = 0
    width = 0, height = 0
}

snippets = Array.new
current = Var.new 1
focus = Var.new true
snippets.add(Snippet("print \"hello world!\""))

running = Var.new null
internal_print = ->
    if not $running: return print("ERROR:", args.join(" "))
    $running.output.input(args.join(" "))
    $running.output.newline
    return
_env = Env.new(_env.set("print", internal_print))

runsnippet = snippet, n ->
    env = { n == 1 }: _env
          {        }: runsnippet(snippets[n - 1], n - 1)
    insame = env == snippet.inputenv
    snippet.inputenv = env
    if insame and not snippet.changed: return snippet.env.get
    snippet.changed = false
    code = snippet.editor.text
    if insame and snippet.code == code: return snippet.env.get
    snippet.code = code

    snippet.output.clear
    snippet.env.set env
    if not env: return

    catch: e ->
        $running = null
        snippet.output.input("error: $(e.toString)") //"
        snippet.output.error = true
        return
    $running = snippet
    res = eval(snippet.env, code)
    $running = null
    if res:
        r = repr(res)
        { r == "null"   }: snippet.output.input(res)
        {               }: snippet.output.input("$r")
    snippet.output.error = false
    //print "EVAL:", snippet.output.text
    return snippet.env.get

runcode = ->
    runsnippet(snippets[$current], $current)

w.onkey(event ->
    catch: e -> print e.toString
    //print "onkey:", repr(event)
    c = event.key
    ctrl = event.modifiers & 2 == 2
    { ctrl and c == 'Q'  }: w.close
    { ctrl and c == 'R'  }:
        runcode()
        if $focus: snippets[$current].changed = true
    { c == 27            }: $focus = false
    { $focus             }: snippets[$current].editor.onkey(event)
    { c == 38            }: $current = max($current - 1, 1)
    { c == 40            }: $current = min($current + 1, snippets.size + 1)
    { c == 13 or c == 32 }:
        if $current > snippets.size: snippets.add(Snippet())
        snippets[$current].editor._x = 1
        snippets[$current].editor._y = 1
        $focus = true
        snippets[$current].changed = true
    { event.input.size == 1 }:
        if $current > snippets.size: snippets.add(Snippet())
        snippets[$current].editor._x = 1
        snippets[$current].editor._y = 1
        $focus = true
        snippets[$current].changed = true
        snippets[$current].editor.onkey(event)
    {                    }: null
    b.redraw
    // debug
    /*
    print "----", b.width, b.height
    snippets.each: snippet, n ->
        print n, "snippet", snippet.y, snippet.height, snippet.editor.text.split("\n")[1]
    */
)

w.onmouse(x, y, buttons, clicks ->
    catch: e -> print e.toString
    snippets.each: snippet, n ->
        if snippet.y - 5 <= y and y <= snippet.y + snippet.height + 5:
            $focus = true
            snippet.changed = true
            $current = n
            snippet.editor.onmouse(x - snippet.x, y - snippet.y, buttons, clicks)
            b.redraw
            return
    last = snippets[-1]
    if y >= last.y + last.height + 5:
        if last.editor.text.trim.size == 0:
            $focus = true
            last.changed = true
            $current = snippets.size
            last.editor.onmouse(x - last.x, y - last.y, buttons, clicks)
            b.redraw
            return
        snippets.add(Snippet())
        $focus = true
        $current = snippets.size
        b.redraw
)

b.ondraw(g ->
    catch: e -> print e.toString
    g.color(0.95,0.95,0.95)
    g.rectangle(0, 0, b.width, b.height)
    g.fill
    y = Var.new 10
    snippets.each: snippet, n ->
        width, height = snippet.editor.size
        height = max(height, FHEIGHT)
        width = max(width, FWIDTH)
        snippet.x = 10
        snippet.y = $y
        snippet.width = width
        snippet.height = height
        g.save
        g.translate(snippet.x, snippet.y)
        snippet.editor.cursor = false
        snippet.editor.x = 10
        snippet.editor.y = $y
        if n == $current:
            g.rectangle(0, 0, width, height)
            g.color 1,1,1
            g.fill
            if $focus: snippet.editor.cursor = true
        snippet.editor.render(g)
        g.restore
        $y += height + 5

        width, height = snippet.output.size
        g.save
        g.translate(10, $y)
        if snippet.output.error:
            g.color 1,0.9,0.9
            g.rectangle(0, 0, width, height)
            g.fill
        snippet.output.render(g)
        g.restore
        $y += height + 10

    if $current > snippets.size:
        g.color 1,1,1
        g.rectangle(10, $y, FWIDTH, FHEIGHT)
        g.fill
)
null

