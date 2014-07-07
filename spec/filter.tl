test "remove matching elements":
    assert [1,2,1].filter(1) == [2]
    assert [1,2,1].filter(2) == [1,1]
    assert [1,2,1].filter(3) == [1,2,1]

test "remove null":
    assert [1,2,null].filter(null) == [1,2]
    assert [null].filter(null) == []

test "remove match function":
    assert [1,2,3,4,5,6].filter(block=x -> x > 2) == [1,2]

test "remove match block":
    r = [1,2,3,4,5,6].filter: x -> x > 2
    assert r == [1,2]

test "remove functions without calling":
    t = -> false
    assert [t, t, 1].filter(t) == [1]

test "remove functions without calling":
    t = -> true
    assert [t, t, 1].filter(t) == [1]

