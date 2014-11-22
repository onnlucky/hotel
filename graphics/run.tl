w = Window.new(500, 500)
w.title("game test")
b = Box.new(0, 0, w.width, w.height)
w.add(b)
w.onresize(width, height -> b.width(width); b.height(height))

app = widget.App(w, b)
view = widget.Vertical()
app.setView(view)

debug = false

map = [
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,1,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1]
    [1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1]
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
]
width = map[1].size
height = map.size
game = map.map(row -> Array.new())

// TODO cannot be global mutable ...
nav = map.map(row -> Array.new())

pass = x, y ->
    return map[y][x] == 0

moves = [[-1,0],[1,0],[0,-1],[0,1]]
move = p, m -> [p[1] + m[1], p[2] + m[2]]
getnav = p ->
    return nav[p[2]][p[1]]
getnavm = p, m ->
    x = p[1] + m[1]
    y = p[2] + m[2]
    if x < 1 or x > width: return 100_000
    if y < 1 or y > height: return 100_000
    return nav[y][x] or 100_000

navfrom = ps, n ->
    nc = Array.new
    ps.each(p ->
        x = p[1]
        y = p[2]
        if x - 1 >= 1 and not nav[y][x - 1] and pass(x - 1, y): nav[y][x - 1] = n; nc.add([x - 1, y])
        if x + 1 <= width and not nav[y][x + 1] and pass(x + 1, y): nav[y][x + 1] = n; nc.add([x + 1, y])
        if y - 1 >= 1 and not nav[y - 1][x] and pass(x, y - 1): nav[y - 1][x] = n; nc.add([x, y - 1])
        if y + 1 <= height and not nav[y + 1][x] and pass(x, y + 1): nav[y + 1][x] = n; nc.add([x, y + 1])
    )
    if nc.size == 0: return
    navfrom(nc, n + 1)


down1 = p ->
    places = moves.map(m -> getnavm(p, m))
    return move(p, moves[places.find(places.min)])
down2 = p ->
    places = moves.map(m -> getnavm(p, m))
    return move(p, moves.reverse[places.reverse.find(places.min)])

pathto = p, ps ->
    ps = ps.add(p)
    if getnav(p) == 0: return ps
    p1 = { random(2) > 0 }: down1(p)
         {               }: down2(p)
    if not p1: return ps
    pathto(p1, ps)

findpath = p1, p2 ->
    nav.each(row -> row.clear)
    nav[p1[2]][p1[1]] = 0
    navfrom([p1], 1)
    return pathto(p2, [])

draw = g ->
    g.setFont("arial", 10)
    map.each: row, y ->
        row.each: cell, x ->
            { cell == 1 }:
                g.color(0)
            {           }:
                g.color(200,200,200)
            g.rectangle(x * 10 + 1, y * 10 + 1, 8, 8).fill
            if debug:
                n = nav[y][x]
                if n: g.moveTo(x * 10, y * 10 + 8).color(200).fillText("$n", x * 10, y * 10)
    game.each: row, y ->
        row.each: cell, x ->
            g.color(0,0,200)
            g.rectangle(x * 10 + 1, y * 10 + 1, 8, 8).fill

units = Array.new

drawunits = g ->
    units.each(unit ->
        { unit.type == "energy" }:
            g.rectangle(unit.x * 10 + 1, unit.y * 10 + 1, 8, 8).color(0.7,0.7,0.1).fill
        { unit.type == "base" }:
            g.rectangle(unit.x * 10 + 1, unit.y * 10 + 1, 8, 8).color(0,220,220,0.5).stroke
        { }:
            g.arc(unit.x * 10 + 5, unit.y * 10 + 5, 4).color(0,0,200,0.5).fill
    )

drawstats = g ->
    g.color.setFont("arial", 16)
    g.moveTo(250, 50)
    units.each(unit ->
        { unit.type == "base" }:
            g.fillText("base: $(unit.state.energy)") //"
        { }: null
    )

moveunits = ->
    if debug: print "move", units.size
    units.each(unit ->
        cmd = unit.cmd
        { not cmd        }: null
        { isList(cmd)    }: unit.state.x = cmd[1]
                            unit.state.y = cmd[2]
        { cmd == "left"  }: unit.state.x -= 1
        { cmd == "right" }: unit.state.x += 1
        { cmd == "up"    }: unit.state.y -= 1
        { cmd == "down"  }: unit.state.y += 1
    )

Unit = x, y ->
    u = {
        type = "unit"
        cmdq = Queue.new
        state = _Mutable_new {
            {x, y}
            task = null
            energy = 0
        }
        x = -> this.state.x
        y = -> this.state.y
        pos = -> [this.state.x, this.state.y]
        cmd = ->
            if not this.state.task: return this.cmdq.poll
            this.cmdq.get
        harvest = ->
            this.state.energy += 50
        transfer = to ->
            to.state.energy += this.state.energy
            this.state.energy = 0
        move = dir -> this.cmdq.add(dir)
        moveTo = x, y ->
            to = { isNumber(x) and isNumber(y) }: [x, y]
                 { isList(x)                   }: x
                 {                             }: x?pos
            ps = findpath(this.pos, to)
            if not ps: return
            ps.reverse[2:].each: p ->
                this.cmdq.add(p)
        setTask = fn ->
            fn = fn or args.block
            if this.state.task: this.state.task.stop
            this.state.task = !(
                (
                    catch: e -> print e.toString
                    fn()
                )
                this.state.task = null
                this.cmdq.add(null)
            )
    }
    units.add(u)

Base = x, y ->
    b = {
        type = "base"
        cmdq = Queue.new
        state = _Mutable_new {
            energy = 300
        }
        {x, y}
        pos = -> [this.x, this.y]
        new = ->
            if this.state.energy < 100: return
            this.state.energy -= 100
            this.cmdq.add(null)
            Unit(this.x, this.y)
        cmd = -> this.cmdq.poll
    }
    units.add(b)

Energy = x, y ->
    e = {
        type = "energy"
        {x, y}
        pos = -> [this.x, this.y]
        cmd = -> null
    }
    units.add(e)

w.onkey(event ->
    catch: e -> print e.toString
    print repr event
    if event.key == 81 and event.modifiers == 2: w.close; return
    app.onkey(event)
)

w.onmouse(x, y, buttons, clicks ->
    catch: e -> print e.toString
    print x, y, buttons, clicks
    view.onmouse({{x,y,buttons,clicks}}, x, y)
)

b.ondraw(g ->
    catch: e -> print e.toString
    view.size(g, b.width, b.height)
    view.draw(g, b.width, b.height)
    draw(g)
    drawunits(g)
    drawstats(g)
)
w.focus

!(
    catch: e -> print e.toString
    loop:
        sleep(0.33)
        moveunits()
        b.redraw
)

energy = Energy(3, 3)
base = Base(10, 10)

view.add(widget.Space(220))

var editor = null
process = ->
    editor.readonly = true
    editor.cursor = false
    code = editor.text
    print "PROCESS:", repr(code)

    out = view.add(Editor("", readonly=true,zeroheight=true))
    _print = -> out.addLines(args.join(" ").split("\n"))
    !(
        catch: e -> print e.toString
        code.eval({{energy,base},print=_print})
    )

    editor = view.add(Editor(""))
    editor.onsubmit = process

example =
"u1 = base.new()
u1.setTask(->
 loop:
   u1.moveTo(energy)
   u1.harvest()
   u1.moveTo(base)
   u1.transfer(base)
)
"

editor = view.add(Editor(example))
editor.onsubmit = process

