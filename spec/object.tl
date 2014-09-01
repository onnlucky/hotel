test "merge":
    o = Object.merge({a=42},{b=100},{a=42})
    assert o.a == 42
    assert o.b == 100
    assert Object.size(o) == 2
