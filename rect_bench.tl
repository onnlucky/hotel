COUNT = 10000
log.info("rect bench starting", COUNT)
start = time()

RectClass = {
    top = -> this.y
    bottom = -> this.y + this.height
    left = -> this.x
    right = -> this.x + this.width
    area = -> this.width * this.height
}
Rect = x, y, width, height -> {class=RectClass, {x, y, width, height}}

rects = Array.new
COUNT.times: n ->
    rects.add(Rect(n // 5 * 10, n % 5 * 10, 10, 10))
log.info("created")

var total_area = 0
rects.each: r -> total_area += r.area
log.info("total area:", total_area)

rects.randomize
log.info("shuffled")

rects.sort(l, r -> l.x < r.x or (l.x == r.x and l.y < r.y))
log.info("sorted")
log.info("total time", time() - start, "seconds")

(#

Square = {}

in = "
Test = { a ->
    extend Square(a)
    b = 1
    var c = 2
    print = -> print b
    class DEFAULT = 3
    class method = -> print this.DEFAULT
}
"
testcompiler.load(in, _env_current()).disasm

###
_constructor = class, this, a ->
    _inherit(class, 1, this, a)
    _setfield(this, "b", 1)
    _setfield(this, "c", 2)
    return this
mutable = true # Square.mutable or OtherInherit.mutable
inherits = [Square]
fields = Set(b, c) + Square.fields
methods = {
    print = -> print b
}
statics = {
    DEFAULT = 3
    method = print this.DEFAULT
}
Test = _class("Test", _constructor, mutable, inherits, fields, methods, statics)

Test.DEFAULT
Test.class.fields
this.class.DEFAULT

