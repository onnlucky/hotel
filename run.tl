//comp.load(io.File("bad.tl").readString, _env_current()).run

Point = { x, y ->
    toString = -> "Point{$x,$y}"
    moveTo = dx, dy -> Object.set(this, "x", x + dx, "y", y + dy)
}
Square = { x, y, width, height ->
    extend Point
    toString = -> "Square{$x,$y,$width,$height}"
}
Foo = { -> }

s = Square(10, 10, 50, 50).moveTo(10,10)
print s.toString

print isa(s, Square)
print isa(s, Point)
print isa(s, Foo)
print isa("hello", String)
