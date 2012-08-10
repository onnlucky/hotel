b1 = Box.new(0, 0, 100, 100)
b1.ondraw({ g ->
    g.color 255, 0, 0
    g.arc 50, 50, 50
    g.fill
})

b2 = Box.new(0, 0, 100, 100)
b2.ondraw({ g ->
    g.setFont "Arial", 16
    g.color 0, 0, 0
    g.moveTo 10, 16
    g.fillText "hello world!"
})

TextField = { ->
    left = Var.new ""
    right = Var.new ""

    box = Box.new(100, 100, 100, 16)
    onkey = { e ->
        print "ONKEY!!"
        /*
        if e.key == KeyEvent.LEFT and left.get.size > 0: {
            right.set left.get.slice(-1).cat(right.get)
            left.set left.get.slice(0, -1)
            box.dirty
            return
        }
        if e.key == KeyEvent.RIGHT and right.get.size > 0: {
            left.set left.get.cat(right.get.slice(0, 1))
            right.set right.get.slice(1)
            box.dirty
            return
        }
        if e.key == KeyEvent.BACKSPACE: {
            left.set left.get.slice(0, -1)
            box.dirty
            return
        }
        */
        if e.input: left.set left.get.cat(e.input); box.redraw
    }
    box.ondraw { g ->
        g.setFont "Arial", 16
        g.color 255, 255, 255
        g.rectangle 0, 0, box.width, box.height
        g.fill

        g.color 0, 0, 0

        g.moveTo 1, 15
        g.fillText left.get
        // cursor
        g.moveToRel 0, -14
        g.lineToRel 0, 13
        g.moveToRel 0, 1
        g.stroke
        g.fillText right.get
    }

    { text: { -> left.get.cat(right.get) }, box: box, onkey: onkey }
}

w = Window.new 400, 400
w.title "Hello World!"
w.focus
w.add b1
w.add b2

text = TextField()
w.add text.box
w.onkey { e -> text.onkey e }

