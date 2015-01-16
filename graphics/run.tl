w = Window.new(500, 500)
w.title("scratchpad")
b = Box.new(0, 0, w.width, w.height)
w.add(b)
w.onresize(width, height -> b.width(width); b.height(height))

app = widget.App(w, b)
view = widget.Vertical()
app.setView(view)

w.onkey(event ->
    catch: e -> print e.toString
    //print repr event
    if event.key == 81 and event.modifiers == 2: w.close; return
    app.onkey(event)
)

w.onmouse(x, y, buttons, clicks ->
    catch: e -> print e.toString
    //print x, y, buttons, clicks
    view.onmouse({{x,y,buttons,clicks}}, x, y)
)

w.onmousescroll(dx, dy ->
    catch: e -> print e.toString
    view.onmousescroll(dx, dy)
)

b.ondraw(g ->
    catch: e -> print e.toString
    view.size(g, b.width, b.height)
    view.draw(g, b.width, b.height)
)
w.focus

divs = Array.new

var Div = null
DivClass = {
    focus = -> this.editor.setFocus
    prev = -> divs[divs.find(this) - 1]
    next = -> divs[divs.find(this) + 1]
    lastenv = ->
        env, new = this.prev?lastenv
        if new: this.run; return this.scope, true
        if not this.dirty: return this.scope, false
        ncode = this.editor.text.trim
        if this.code == ncode: this.dirty = false; return this.scope, false
        this.run
        return this.scope, true
    run = ->
        this.scope = null
        this.output.clear
        code = this.editor.text.trim
        this.code = code
        output = this.output
        env = this.prev?lastenv or true
        _print = -> output.addLines(args.map(a -> toString(a)).join(" ").split("\n"))
        catch: e -> _print(e)
        frame = code.eval({print=_print}, frame=env)
        res = frame.run
        if not frame.allStored and isDefined(res) and res != null: _print(res)
        this.scope = frame
    key = event ->
        cmd = event.modifiers == 2
        { cmd and event.key == 'N' }: Div.new(divs.find(this)).focus
        { cmd and event.key == 'D' }: if divs.size == 1: return
                                      at = divs.find(this)
                                      divs.remove(at)
                                      view.remove(this.editor)
                                      view.remove(this.output)
                                      divs[at]?focus or divs[at - 1]?focus
        { cmd and event.key == 'R' }: divs[-1].run; true
        { cmd and event.key == 38  }: this.prev?focus; true
        { cmd and event.key == 40  }: this.next?focus; true
        { event.key == 38 }:
            if this.editor.currentLine == 1: this.prev?focus; return true
            false
        { event.key == 40 }:
            if this.editor.currentLine == this.editor.lines: this.next?focus; return true
            if this.editor.lines == 0: this.next?focus; return true
            false
        {                          }: this.dirty = true; false
}
Div = {
    new = at, code ->
        o = _Mutable_new({
            class = DivClass
            editor = Editor(code or "")
            output = Editor("", readonly=true, zeroheight=false)
            dirty = false
            code = null
            scope = null
        })
        o.editor.keyfilter = key -> o.key(key)
        divs.insert(at + 1, o)
        view.insert(at * 2 + 1, o.editor)
        view.insert(at * 2 + 2, o.output)
        return o
}

Div.new(0).focus

