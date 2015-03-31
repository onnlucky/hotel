test "merge":
    o = Object.merge({a=42},{b=100},{a=42})
    assert o.a == 42
    assert o.b == 100
    assert Object.size(o) == 2

test "hashing objects":
    h1 = Object.hash({a=42})
    h2 = Object.hash({a=42,b=100})
    h3 = Object.hash({a=42,b=100,c=1000})
    h4 = Object.hash(Object.set({}, {b=100}, {a=42}))
    assert h1 != h2
    assert h2 != h3
    assert h4 == h2

test "hashing objects with bad things throws":
    var haderror = false
    (
        catch: e -> haderror = true
        Object.hash({a=(->null)})
    )
    assert haderror
