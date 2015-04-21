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

