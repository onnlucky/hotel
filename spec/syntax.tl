test "multiple value returns":
    x, y = (->return 42, 43)()
    assert x == 42 and y == 43

test "multiple value returns assigning to variables":
    var x
    var y
    (
        # in sub scope, test they are null, then assign
        assert x == null and y == null
        x, y = (->return 42, 43)()
    )
    assert x == 42 and y == 43

test "multiple value returns assigning to new variables":
    x, var y, z = (->return 42, 43)()
    assert_mutable_y = -> assert y == 100
    assert x == 42
    assert y == 43
    assert z == null
    y = 100
    assert_mutable_y()

test "multiple value returns assigning to fields":
    # an anonymous class with mutable's of which we make an instance
    o = {-> var x; var y }()
    o.x, o.y = (->return 42, 43)()
    assert o.x == 42 and o.y == 43

test "negative operator works for many different expressions":
    o = { x = 42 }
    f = n -> n
    assert -42 == -1 * 42
    assert -o.x == -42
    assert -f(42) == -42
    assert -f(-42) == 42
    assert -f.call(Args(42)) == -42
    assert -(42) == -42
    # TODO? can this work?
    # assert -(n->n)(42) == -42

