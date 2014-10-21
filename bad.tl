_disasm()
Box = { x ->
    var $y = x
    add = n -> y = y + n
}

b = Box(10)
b.add(1)
b.add(3)
b2 = Box(10)
b2.add(10)
b2.add(30)
print b.y.get
print b2.y.get
