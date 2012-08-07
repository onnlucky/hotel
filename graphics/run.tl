w = Window.new 400, 400
w.setTitle "Hello World!"
w.show

g = Graphics.new w.width, w.height
g.save
g.color 0, 0, 0
g.setFont "Arial", 16
tw, th = g.measureText "hello world!"
g.translate(((g.width) - tw) / 2, ((g.height) - th) / 2)
g.rotate 30
g.fillText "hello world!"
g.restore

g.color 255, 0, 0
g.arc 0, 0, 50
g.fill

w.draw g

s = Scene.new
n = s.newNode
n.width(256)
n.height(256)
s.add(n)

/*
w.ondraw: {
    g.resize w.width, w.height
    w.draw g
}

while true: {
    mx, my = w.mouse
    x, y, width, height = w.frame
    g.clear
    g.resize(width, height)
    g.rgb(255, 0, 0)
    g.circle(mx - x, my - y, 50)
    w.draw g
}

