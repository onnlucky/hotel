test "objects compare equal if structurally equal":
    assert {} == {}
    assert {a=10,b=10} == {b=10,a=10}

test "objects are hashable, if all values are hashable":
    h1 = Object.hash({a=42})
    h2 = Object.hash({a=42,b=100})
    h3 = Object.hash({a=42,b=100,c=1000})
    h4 = Object.hash(Object.set({}, {b=100}, {a=42}))
    assert h1 != h2
    assert h2 != h3
    assert h4 == h2

test "objects are not hashable, if they contain a non hashable value":
    var haderror = false
    (
        catch: e -> haderror = true
        Object.hash({a=(->null)})
    )
    assert haderror

test "merge":
    o = Object.merge({a=42},{b=100},{a=42})
    assert o.a == 42
    assert o.b == 100
    assert Object.size(o) == 2

test "we can delete from using Object.del":
    assert Object.del({a=10}, "a") == {}

# TODO del should support io lists
test "Object.del ignores falsy values":
    orig = {a=10,b=20,c=30,d=40,z=100,e=50}
    no_abcz = Object.del(orig, "a", "b", null, "c", [][1], false, "z") # [][1] returns undefined
    assert no_abcz == {d=40,e=50}
