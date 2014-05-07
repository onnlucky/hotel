w = Window.new 400, 400
w.title "Hello World!"

lenna = Image.new "lenna.png"
b3 = Box.new(100, 100, 200, 200)
b3.center(100, 100)
b3.rotate(0.2)
b3.scale(0.1, 0.1)
b3.ondraw(g -> g.image lenna, 0, 0, 200, 200)
!(
    last = Var.new now()
    while not w.isClosed:
        wait 0.030
        n = now()
        dt = n - last.get
        last.set(n)
        b3.rotate(b3.rotate + dt)
        b3.scale(1+sin(n), 1+sin(n))
)
w.add b3


b1 = Box.new(100, 100)
b1.ondraw(g ->
    g.color 255, 0, 0
    g.arc 50, 50, 50
    g.fill
)
w.add b1
w.focus

b2 = Box.new(100, 100)
b2.ondraw(g ->
    g.setFont "Arial", 16
    g.color 0, 0, 0
    g.moveTo 10, 16
    g.fillText "hello world!"
)
w.add b2

TextField = ->
    left = Var.new ""
    right = Var.new ""

    box = Box.new(100, 100, 100, 116)
    box.rotate(0.2)
    box.alpha(0.5)
    onkey = (e ->
        catch: x -> print x.toString
        if e.key == 37 and left.get.size > 0:
            right.set left.get.slice(-1).cat(right.get)
            left.set left.get.slice(0, -2)
            box.redraw
            return
        if e.key == 39 and right.get.size > 0:
            left.set left.get.cat(right.get.slice(1, 1))
            right.set right.get.slice(2)
            box.redraw
            return
        if e.key == 8:
            left.set left.get.slice(1, -2)
            box.redraw
            return
        if e.key == 27:
            w.close
            return
        if e.input:
            left.set left.get.cat(e.input)
            box.redraw
    )
    box.ondraw(g ->
        g.setFont "Arial", 16
        g.color 1, 1, 1, 0.5
        g.rectangle 0, 0, box.width, box.height
        g.fill

        g.color 0, 0, 0

        g.moveTo 11, 55
        g.fillText left.get
        // cursor
        g.lineToRel 0, -14
        g.moveToRel 0, 14
        g.stroke true
        g.fillText right.get
    )
    { text=(-> left.get.cat(right.get)), box=box, onkey=onkey }

wait 1

text = TextField()
w.add text.box
w.onkey(e -> text.onkey(e))
null

