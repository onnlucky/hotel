test "objects resolve methods then fields":
    Test = { ->
        x = 42
        x = -> 100
    }
    assert Test().x == 100

test "objects resolve methods in extends left/depth first, then fields":
    Base = { ->
        x = -> 42
        y = 43
    }
    Test = { ->
        extend Base
        x = 100
        y = -> 200
    }
    assert Test().x == 42
    assert Test().y == 200

test "objects resolve left/depth first":
    Base1 = { -> x = (-> 100) }
    Base2 = { -> x = (-> 200) }
    Base3 = { -> x = (-> 300) }
    Base4 = { -> x = (-> 400) }
    Middle = { -> extend Base1; extend Base2; extend Base3 }
    Test = { -> extend Middle; extend Base4 }
    assert Test().x == 100

test "objects resolve left/depth first and super works over multiple inheritance":
    Base1 = { -> x = (-> 100); base1 = 1 }
    Base2 = { -> x = (-> 200); base2 = (-> 2) }
    Base3 = { -> x = (-> 300); base3 = 3 }
    Base4 = { -> x = (-> 400); base4 = (-> 4) }
    Middle = { -> extend Base1; extend Base2; extend Base3 }
    Test = { -> extend Middle; extend Base4; base3 = (-> super.base3); base4 = (-> super.base4) }
    assert Test().x == 100
    assert Test().base1 == 1
    assert Test().base2 == 2
    assert Test().base3 == 3
    assert Test().base4 == 4

test "objects can share fields, and last write wins":
    # This is a tradeoff, can also throw an error on shared fields, but field
    # initialization is simply observable in constructors that use "this"
    # before fully initialized. Advantage is diamond inheritance kinda works.
    # And as a dynamic language, we are not going to interfere as the author
    # mixes up field names.
    Base1 = { -> x = 100 }
    Base2 = { -> x = 200 }
    Base3 = { -> x = 300 }
    Base4 = { -> x = 400 }
    Middle = { -> extend Base1; extend Base2; extend Base3 }
    Test = { -> extend Middle; extend Base4 }
    assert Test().x == 400

test "super works for fields and methods":
    Base1 = { -> x = 100; m1 = (-> 100); m2 = (-> this.x) }
    assert Base1().x == 100
    assert Base1().m1 == 100
    assert Base1().m2 == 100

    Base2 = { -> x = 200; m1 = (-> 200); m2 = (-> this.x) }
    Base3 = { -> x = 300; m1 = (-> 300); m2 = (-> this.x) }
    Base4 = { -> x = 400; m1 = (-> 400); m2 = (-> this.x) }
    Middle = { -> extend Base1; extend Base2; extend Base3 }
    Test = { -> extend Middle; extend Base4; tm1 = (-> super.m1); tm2 = (-> super.m2); tmx = (-> super.x) }
    assert Test().x == 400  # last write wins for shared fields
    assert Test().m1 == 100 # method resolves left/depth first
    assert Test().m2 == 400 # method resolves left/depth first, but returning last write shared field
    assert Test().tmx == 400 # same, but through super
    assert Test().tm1 == 100 # idem
    assert Test().tm2 == 400 # idem

test "super when field or method is not in super, is an error":
    Base = { -> f1 = 10; m1 = (-> 20) }
    Test = { -> extend Base; f1 = (-> super.f1); m1 = (-> super.m1); tf1 = (-> super.f1); tm1 = (-> super.m1); tf2 = (-> super.f2); tm2 = (-> super.m2) }
    assert Test().f1 == 10
    assert Test().m1 == 20
    assert try(Test().tf1) == 10
    assert try(Test().tm1) == 20
    assert try(Test().tf2) == null
    assert try(Test().tm2) == null


###
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

