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
    #print repr event
    if event.key == 81 and event.modifiers == 2: w.close; return
    app.onkey(event)
)

w.onmouse(x, y, buttons, clicks ->
    catch: e -> print e.toString
    #print x, y, buttons, clicks
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

colors = {
    light = [0.7,0.7,0.7]
    output = [0.2,0.2,0.2]
    text = [0.05, 0.05, 0.05]

    normalbg = [0.9, 0.9, 0.9]
    errorbg = [1, 0.9, 0.9]
}

var running = null

# TODO run each div in a task, cancel those and reflect it in the output
stop = ->
    if not running: return
    running.abort
    running = null

run = ls ->
    stop()
    task = Task.new
    task.run:
        catch: e -> print toString(e)
        var scope = true
        ls.each(run -> scope = run(scope))
    running = task

divs = Array.new

var Div = null
DivClass = {
    focus = -> this.editor.setFocus
    prev = -> divs[divs.find(this) - 1]
    next = -> divs[divs.find(this) + 1]
    preprun = ->
        code = this.editor.text.trim
        output = this.output
        output.color = colors.light
        # returns a function, so that we release "this" and allow rendering of the ui
        scope ->
            # use cached result if possible
            if scope == this.inscope and this.code == code and this.outscope:
                output.color = colors.output
                return this.outscope

            output.clear
            output.background = colors.normalbg
            output.color = colors.output

            # exit if error
            if not scope: return null

            this.inscope = scope
            this.code = code
            this.outscope = null
            _print = -> output.addLines(args.map(a -> toString(a)).join(" ").split("\n")); null
            catch: e ->
                output.background = colors.errorbg
                _print(e)
            frame = code.eval({print=_print}, frame=scope)
            res = frame.run
            if not frame.allStored and isDefined(res) and res != null: _print(res)
            this.outscope = frame

    key = event ->
        cmd = event.modifiers == 2
        { cmd and event.key == 'N' }: Div.new(divs.find(this)).focus
        { cmd and event.key == 'D' }: if divs.size == 1: return
                                      at = divs.find(this)
                                      divs.remove(at)
                                      view.remove(this.editor)
                                      view.remove(this.output)
                                      divs[at]?focus or divs[at - 1]?focus
        { cmd and event.key == 'R' }: this.code = null # clear at least our cache
                                      run(divs.map(div -> div.preprun))
                                      true
        { cmd and event.key == 'S' }: stop(); true
        { cmd and event.key == 38  }: this.prev?focus; true
        { cmd and event.key == 40  }: this.next?focus; true
        { event.key == 38 }:
            if this.editor.currentLine == 1: this.prev?focus; return true
            false
        { event.key == 40 }:
            if this.editor.currentLine == this.editor.lines: this.next?focus; return true
            if this.editor.lines == 0: this.next?focus; return true
            false
        {                          }: false
}
Div = {
    new = at, code ->
        o = _Mutable_new({
            class = DivClass
            editor = Editor(code or "")
            output = Editor("", readonly=true, zeroheight=false)
            inscope = null
            code = null
            outscope = null
        })
        o.editor.keyfilter = key -> o.key(key)
        divs.insert(at + 1, o)
        view.insert(at * 2 + 1, o.editor)
        view.insert(at * 2 + 2, o.output)
        return o
}

Div.new(0, "sleep 10").focus

