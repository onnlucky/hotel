w = Window.new
w.setTitle "Hello World!"
w.show

loop = { c ->
    mx, my = w.mouse
    x, y, width, height = w.frame
    g = w.graphics
    g.clear
    g.rgb(255, 0, 0)
    g.circle(mx - x, my - y, 50)
    w.draw g
    sleep 40
    goto loop((c + 1) % 255)
}
loop(0)

