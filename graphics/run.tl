w = Window.new 400, 400
w.title "Hello World!"

b = Box.new 0, 0, 100, 100
w.add(b)
t = Text.new(b)

b = Box.new 0, 0, 200, 200
b.ondraw(g -> g.rectangle(0, 0, b.width, b.height); g.fill)
w.add(b)

w.focus

