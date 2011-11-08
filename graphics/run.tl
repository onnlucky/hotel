w = Window.new;
w.show

loop = { c ->
    g = w.graphics
    g.rgb(c, 0, 0)
    g.circle(100, 10 + c, 50)
    w.draw g
    sleep 40
    goto loop((c + 1) % 255)
}
loop(0)

