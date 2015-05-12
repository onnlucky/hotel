test "objects resolve methods then fields":
    Test = { ->
        x = 42
        x = -> 100
    }
    assert Test().x == 100
    assert Test() isa Test

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

    # test isa operator
    assert Test() isa Test
    assert Test() isa Middle
    assert Test() isa Base1
    assert Test() isa Base2
    assert Test() isa Base3
    assert Test() isa Base4
    assert not Middle() isa Base4
    assert not Middle() isa Test

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

test "safe operator works for objects":
    Base = { ->
        f = 100
        m = -> 200
    }
    Test = { -> extend Base; f2 = 300; m2 = (-> 400) }
    o = Test()
    assert o.f == 100
    assert o?f == 100
    assert o.m == 200
    assert o?m == 200
    assert o.f2 == 300
    assert o?f2 == 300
    assert o.m2 == 400
    assert o?m2 == 400

    # first will throw, second will return null and so will the try wrapper
    assert try(o.f3, "otherwise") == "otherwise"
    assert try(o?f3, "otherwise") == null

    Down = { -> extend Test; m3 = (-> super.m3) }
    assert try(Down().f2, "otherwise") == 300
    assert try(Down()?f2, "otherwise") == 300
    assert try(Down().f3, "otherwise") == "otherwise"
    assert try(Down()?f3, "otherwise") == null
    assert try(Down().m3, "otherwise") == "otherwise"
    assert try(Down()?m3, "otherwise") == "otherwise" # m3 exists, so safe operator does not help

test "named method":
    Test = { ->
        tocall = in -> in + 2
    }
    t = Test()
    assert t.tocall(1, in=40, 2) == 42
    assert t.tocall(40, x=1, y=2) == 42

test "named constructors":
    Test = { x, this.y -> z = x or 200 }
    t = Test(y=100)
    assert t.y == 100
    assert try(t.x, "this") == "this"
    assert t.z == 200

test "named super":
    Base = { ->
        tocall = in -> in + 2
    }
    Test = { ->
        extend Base
        tocall = -> super.tocall(1, in=40, 2)
    }
    assert Test().tocall == 42

test "named class method":
    Test = { ->
        class foo = in -> in + 2
        class bar = 42
    }
    assert Test.foo(in=40, 1, 2) == 42
    assert Test.foo(1, in=40, 2) == 42
    assert Test.foo(1, 2, in=40) == 42
    assert Test.foo(40, x=1, y=1) == 42
    assert Test.foo(x=1, 40, y=1) == 42
    assert Test.foo(x=1, y=1, 40) == 42

    assert Test.bar == 42
    assert Test.bar(100) == 42
    assert Test.bar(x=100) == 42
    assert Test.bar(1, 2, 3, x=1, y=2, z=3) == 42

