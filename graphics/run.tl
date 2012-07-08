w = Window.new 400, 400
w.setTitle "Hello World!"

g = Graphics.new w.width, w.height
g.rgb(255, 0, 0)
g.circle(100, 100, 50)
//g.text "hello world!"
w.draw g
w.show

/*
w.ondraw: {
    g.resize w.width, w.height
    w.draw g
}
*/

while true: {
    mx, my = w.mouse
    x, y, width, height = w.frame
    g.clear
    g.resize(width, height)
    g.rgb(255, 0, 0)
    g.circle(mx - x, my - y, 50)
    w.draw g
}

