Base = { y ->
    y = y
    dump = -> print "Base{", this.y, "}"
}
Full = { x, y ->
    x = x
    extend Base(y)
    dump = -> print "Full{", this.x, this.y, "}"
}

o = Full(42, 43)
o.dump
