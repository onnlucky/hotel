w = Window.new
w.setTitle "Hello World!"
w.show

w2 = Window.new
w2.setTitle "oh hi!"
w2.show
!sleep 2000; w2.hide

loop = { c ->
    mx, my = w.mouse
    x, y, width, height = w.frame
    g = w.graphics
    g.rgb(0, 0, 0)
    g.circle(mx - x, my - y, 50)
    w.draw g
    sleep 40
    goto loop((c + 1) % 255)
}
loop(0)

