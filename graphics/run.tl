w = Window.new
w.setTitle "Hello World!"
w.show

g = Graphics.new w.width, w.height
g.rgb(255, 0, 0)
g.circle(100, 100, 50)
//g.text "hello world!"
w.draw g

loop = { c ->
    mx, my = w.mouse
    x, y, width, height = w.frame
    g.clear
    g.resize(width, height)
    g.rgb(255, 0, 0)
    g.circle(mx - x, my - y, 50)
    w.draw g
    goto loop((c + 1) % 255)
}
loop(0)

