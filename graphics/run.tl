w = Window.new 400, 400
w.setTitle "Hello World!"
w.show

scene = w.scene
n = scene.newNode
n.x(200)
n.y(200)
n.width(100);
n.height(100);
n.ondraw({ g ->
    g.color 255, 0, 0
    g.arc 50, 50, 50
    g.fill
})
scene.add n

