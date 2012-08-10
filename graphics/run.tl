b1 = Box.new(0, 0, 100, 100)
b1.ondraw({ g ->
    print "circle"
    g.color 255, 0, 0
    g.arc 50, 50, 50
    g.fill
})

b2 = Box.new(0, 0, 100, 100)
b2.ondraw({ g ->
    print "hello world"
    g.color 0, 0, 0
    g.moveTo 10, 10
    g.fillText "hello world!"
})

w = Window.new 400, 400
w.add b1
w.add b2
w.title "Hello World!"

w.focus

