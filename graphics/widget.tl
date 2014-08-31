clamp = value, min, max -> { value < min }: min
                           { max < min   }: min
                           { value > max }: max
                           {             }: value

App = window, box -> _Mutable_new {
    view = null
    app = -> this

    width = -> box.width
    height = -> box.height
    dirty = -> box.redraw

    _focus = null
    focus = ->
        { not this._focus         }: null
        { this._focus.app != this }: null
        {                         }: this._focus
    setFocus = view ->
        this._focus = view
    setView = view ->
        this.view = view
        view.parent = this

    onkey = event ->
        try(this.focus.onkey(event))
}

Widget = text -> _Mutable_new {
    parent = null
    x = 0
    y = 0
    width = 100
    height = 100

    app = -> this.parent.app
    focus = -> try(this.app._focus) == this
    setFocus = -> this.app.setFocus(this)

    size = g, width -> null
    draw = g ->
        g.setFont("Arial", 16)
        g.color(0.9,0.9,0.9)
        if this.focus: g.color(1,0.8,0.8)
        g.rectangle(2, 2, this.width - 4, this.height - 4)
        g.fill
        g.color(0,0,0)
        w, h = g.measureText(text)
        g.moveTo(this.width / 2 - w / 2, this.height / 2 + h / 2)
        g.fillText(text)
    onkey = event ->
        print "onkey:", repr(event)
    onmouse = event, x, y ->
        this.setFocus()
        { event.buttons == 1 }:
            this.width += 10
            this.height += 10
            this.parent.dirty
        { event.buttons == 2 }:
            this.width -= 10
            this.height -= 10
            this.parent.dirty
}

Text = text -> _Mutable_new {
    parent = null
    x = 0
    y = 0
    width = 0
    height = 0
    lines = Array.new

    app = -> this.parent.app

    size = g, width ->
        if this.width == width: return
        this.width = width
        this.lines.clear

        g.setFont("Arial", 16)
        _, height = g.measureText("fg")
        words = text.split(" ")
        sizes = words.map(w -> g.measureText(w + " "))

        size = Var.new 0
        text = Var.new ""
        sizes.each: s, n ->
            if $size + s > width:
                this.lines.add($text)
                $text = ""
                $size = 0
            $text = "$($text)$(words[n]) "
            $size += s
        if $text.size > 0: this.lines.add($text)
        this.width = width
        this.height = this.lines.size * height

    draw = g ->
        g.setFont("Arial", 16)
        _, height = g.measureText("fg")
        g.color(0,0,0)
        x = Var.new height
        this.lines.each: l ->
            g.moveTo(0, $x)
            g.fillText(l)
            $x += height

    onmouse = -> null
}

Vertical = -> _Mutable_new {
    parent = null
    children = Array.new
    _dirty = true
    x = 0
    y = 0
    width = 0
    height = 0

    xscroll = 0
    yscroll = 0

    app = -> this.parent.app
    dirty = ->
        if this._dirty: return
        this._dirty = true
        if this.parent: this.parent.dirty

    draw = g ->
        ymin = this.yscroll
        ymax = this.yscroll + this.parent.height
        xmin = this.xscroll
        xmax = this.xscroll + this.parent.width
        this.children.each: c ->
            if c.y + c.height < ymin and c.y > ymax: continue
            if c.x + c.width < xmin and c.x > xmax: continue
            g.save
            g.translate(c.x - xmin, c.y - ymin)
            g.rectangle(0, 0, c.width, c.height)
            g.clip()
            c.draw(g)
            g.restore

    add = c ->
        c.parent = this
        this.children.add(c)

    size = g, width ->
        this._dirty = false
        this.width = 0
        this.height = 0
        this.children.each: c ->
            c.size(g, width)
            c.x = 0
            c.y = this.height
            this.height += c.height
            this.width = max(this.width, c.width)

    onmouse = event, x, y ->
        this.children.each: c ->
            cx = this.xscroll + x - c.x
            if cx < 0: continue
            if cx > c.width: continue
            cy = this.yscroll + y - c.y
            if cy < 0: continue
            if cy > c.height: continue
            return c.onmouse(event, cx, cy)

    onmousescroll = dx, dy ->
        this.yscroll = clamp(this.yscroll + dy * 4, 0, this.height - this.parent.height)
        this.xscroll = clamp(this.xscroll + dx * 4, 0, this.width - this.parent.width)
        this.dirty
}

{{ App, Text, Widget, Vertical }}
