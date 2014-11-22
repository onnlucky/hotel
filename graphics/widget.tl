DEFAULT_FONT = "Arial"
DEFAULT_FONT_SIZE = 16

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
        if this._focus: this._focus.onkey(event)

    onmousemove = handler -> window.onmousemove(handler)
    clipboardGet = -> window.clipboardGet
    clipboardSet = text -> window.clipboardSet(text)
}

widget_class = {
    app = -> this.parent.app
    focus = -> try(this.app._focus) == this
    setFocus = -> !this.app.setFocus(this)
    setVisible = visible -> this.visible = visible; !try(this.parent.dirty)
    size = g, width -> null
    draw = g, width, height -> null
    onkey = event -> null
    onmouse = event, x, y -> null
}

widget_fields = {
    parent = null
    visible = true
    x = 0
    y = 0
    width = 0
    height = 0
}

Space = height -> _Mutable_new(Object.merge(
    widget_fields
    { height = height or DEFAULT_FONT_SIZE }
    { class = Object.merge(widget_class, {
        size = g, width, height -> this.width = width
    })}
))

Text = text, align, font, size, style, color, background -> _Mutable_new(Object.merge(
    widget_fields
    { text = text, lines = Array.new, linesizes = Array.new }
    { class = Object.merge(widget_class, {
        setText = text ->
            this.text = text
            this.width = 0
            this.parent.dirty
        size = g, width ->
            if this.width == width: return
            this.width = width
            this.lines.clear
            this.linesizes.clear

            g.setFont(font or DEFAULT_FONT, size or DEFAULT_FONT_SIZE, style == "italic", style == "bold")
            height = g.fontSize()
            words = this.text.split(" ")
            sizes = words.map(w -> g.measureText(w + " "))

            var $size = 0
            var $text = ""
            sizes.each: s, n ->
                if $size + s > width:
                    this.lines.add($text)
                    this.linesizes.add($size)
                    $text = ""
                    $size = 0
                $text = "$($text)$(words[n]) "
                $size += s
            if $text.size > 0:
                this.lines.add($text)
                this.linesizes.add($size)
            this.width = width
            this.height = this.lines.size * height + 2

        draw = g, width, height ->
            if background: g.color(background).rectangle(0, 0, width, height).fill

            g.setFont(font or DEFAULT_FONT, size or DEFAULT_FONT_SIZE, style == "italic", style == "bold")
            height = g.fontSize()
            g.color(color or "black")
            var $x = height - 2
            this.lines.each: l, n ->
                { align == "center" }: g.moveTo(width / 2 - this.linesizes[n] / 2, $x)
                { align == "right"  }: g.moveTo(width - this.linesizes[n], $x)
                {                   }: g.moveTo(0, $x)
                g.fillText(l)
                $x += height
    })}
))

Vertical = -> _Mutable_new {
    parent = null
    visible = true
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

    draw = g, width, height ->
        ymin = this.yscroll
        ymax = this.yscroll + this.parent.height
        xmin = this.xscroll
        xmax = this.xscroll + this.parent.width
        this.children.each: c ->
            if not c.visible: continue
            if c.y + c.height < ymin and c.y > ymax: continue
            if c.x + c.width < xmin and c.x > xmax: continue
            g.save
            g.translate(c.x - xmin, c.y - ymin)
            g.rectangle(0, 0, c.width, c.height)
            g.clip()
            c.draw(g, width - (c.x - xmin), height - (c.y - ymin))
            g.restore

    add = c ->
        c.parent = this
        this.children.add(c)

    size = g, width ->
        this._dirty = false
        this.width = 0
        this.height = 0
        this.children.each: c ->
            if not c.visible: continue
            c.size(g, width)
            c.x = 0
            c.y = this.height
            this.height += c.height.ceil
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

{{ widget_class, App, Vertical, Text, Space }}
