Shape = { this.x, this.y ->
    toString = -> "Shape{$(this.x),$(this.y)}"
    dist = -> sqrt(this.x ** 2 + this.y ** 2)
}

Rectangle = { x, y, this.width, this.height ->
    extend Shape(x, y)
    toString = ->
        "Rectangle{$(this.x),$(this.y),$(this.width),$(this.height)}"
    area = ->
        this.width * this.height
    dist = ->
        super.dist - sqrt((this.width / 2) ** 2 + (this.height / 2) ** 2)
}

Circle = { x, y, this.r ->
    extend Shape(x, y)
    toString = ->
        "Circle{$(this.x),$(this.y),$(this.r)}"
    area = ->
        3.14159265359 * this.r ** 2
    dist = ->
        super.dist - this.r
}

r1 = Rectangle(10, 10, 50, 50)
c1 = Circle(100, 0, 10)

assert r1.x == 10, r1.y == 10
assert r1.area == 50 * 50
assert r1.toString == "Rectangle{10,10,50,50}"
assert r1 isa Rectangle
assert r1 isa Shape

assert c1 isa Circle
assert c1 isa Shape
assert c1.area > 20
assert c1.dist == 90

