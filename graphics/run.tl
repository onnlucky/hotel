w = Window.new(500, 500)
w.title("widget test")
b = Box.new(0, 0, w.width, w.height)
w.add(b)
w.onresize(width, height -> b.width(width); b.height(height))

PI = 3.14
TAU = 6.28

dist2 = o1, o2 -> (o1.x - o2.x)^2 + (o1.y - o2.y)^2
dist = o1, o2 -> sqrt((o1.x - o2.x)^2 + (o1.y - o2.y)^2)
angle = o1, o2 -> atan2(o2.y - o1.y, o2.x - o1.x)

PUSH_SPEED = 10

var dragging = null
world = Array.new

nearestEnemy = from, player ->
    var targetdist = 1000_000_000
    var target = null
    world.each(o ->
        if o.type == "player": return
        if o.player == player: return
        d = dist(from, o)
        if d < targetdist:
            targetdist = d
            target = o
    )
    return target

Player = name, x, y, color -> _Mutable_new {
    type = "player"
    {name,x,y,color}
    energy = 100.0
    proto = Array.new
    init = ->
        this.proto.add(Turret(this, 0, 0))
        this.proto.add(Energy(this, 0, 0))
    draw = g ->
        g.color(this.color)
        this.proto.each(p, n -> g.save.translate(n * 30, 0); p.draw(g); g.restore)
        g.fillText("$(this.name) energy=$(this.energy.round)") //"
    update = -> null
}

Turret = player, x, y -> _Mutable_new {
    type = "turret"
    {player,x,y}
    turret = 0.0
    target = null
    energy = 0
    health = 1.0
    draw = g ->
        g.color(0,0.1,0).rectangle(-10,5,20,6).fill
        g.color(0,1,0).rectangle(-10,5,20 * this.health,6).fill
        g.color(this.player.color)
        g.arc(0, 0, 10)
        g.rotate(this.turret)
        g.rectangle(-2, -2, 20, 2)
        g.fill
    update = dt ->
        if this.health <= 0: return false
        if this.energy < 1:
            this.energy += 0.5 * dt
            this.player.energy -= 0.5 * dt
        this.target = nearestEnemy(this, this.player)
        if this.target:
            this.turret = angle(this, this.target)
            d = dist(this, this.target)
            if d < 100 and this.energy >= 0.5:
                this.energy -= 0.5
                this.target.health -= 0.1
}

Energy = player, x, y -> _Mutable_new {
    type = "energy"
    {player,x,y}
    color = [0.6,0,0]
    health = 1.0
    draw = g ->
        g.color(0,0.1,0).rectangle(-10,6,20,7).fill
        g.color(0,1,0).rectangle(-10,6,20 * this.health,7).fill
        g.color(this.player.color)
        g.rectangle(-10, -10, 20, 20)
        g.fill
    update = dt ->
        if this.health <= 0: return false
        p = this.player
        p.energy += 1 * dt
        world.each(o ->
            if o == this: return
            if o.type == "player": return
            if o.player != this.player: return
            d = dist(this, o)
            if d < 50:
                a = angle(this, o)
                o.x += cos(a) * dt * PUSH_SPEED
                o.y += sin(a) * dt * PUSH_SPEED
        )
}

p1 = Player("p1", 10, 20, [1,0,0])
p1.init
p2 = Player("p2", 10, 480, [0,0,1])
p2.init

world.add(p1)
world.add(p2)

!(loop:
    catch: e -> print e.toString
    sleep 0.01
    toremove = Array.new
    world.each: o ->
        if o.update(0.01) == false: toremove.add(o)
    toremove.each: o ->
        world.remove(world.find(o))
    b.redraw
)

w.onkey(event ->
    catch: e -> print e.toString
    print repr event
    if event.key == 81 and event.modifiers == 2: w.close; return
    b.redraw
)
w.onmouse(x, y, buttons, clicks ->
    catch: e -> print e.toString
    print x, y, buttons, clicks

    player = { y < 50  }: p1
             { y > 450 }: p2
             {         }: null
    if player:
        { x >= 30 and x <= 60 }:
            dragging = Turret(player, x, y)
        { x >= 60 and x <= 90 }:
            dragging = Energy(player, x, y)
        {                     }:
            dragging = null

    if not dragging: return
    w.onmousemove(x, y, buttons ->
        dragging.x = x
        dragging.y = y
        if buttons == 0:
            if dragging: world.add(dragging)
            dragging = null
            w.onmousemove(null)
        b.redraw
    )
    b.redraw
)
b.ondraw(g ->
    catch: e -> print e.toString
    world.each: o ->
        g.save
        g.translate(o.x, o.y)
        o.draw(g)
        g.restore
    if dragging:
        g.save
        g.translate(dragging.x, dragging.y)
        dragging.draw(g)
        g.restore
)
!(sleep 0.01; w.focus)
return null

