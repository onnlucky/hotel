Base = { y ->
    y = y
}
Full = { x ->
    x = x
    extend Base(20)
}

o = Full(100)
print o.x
print o.y
