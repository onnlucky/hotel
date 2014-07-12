test "basic compare":
    assert 1 < 2
    assert 1 <= 2
    assert 2 > 1
    assert 2 >= 1

    assert not 1 > 1
    assert not 1 < 1
    assert 1 <= 1
    assert 1 >= 1

    assert "atom" < "you wish"
    assert "foo" > "alan"

test "cannot compare unrelated values":
    // TODO one day, would like undefined to throw if used for anything but comparing
    assert isUndefined(null < 1)
    assert isUndefined(null <= 1)
    assert isUndefined(null > 1)
    assert isUndefined(null >= 1)

    assert isUndefined("hello" < 1)
    assert isUndefined("hello" <= 1)
    assert isUndefined("hello" > 1)
    assert isUndefined("hello" >= 1)

test "total ordering":
    assert compare(null, 1) == -1 or compare(null, 1) == -1
    assert compare("foo", 1) == 1 or compare("foo", 1) == 1
    assert compare("foo", "foo") == 0
    assert compare(1, 2) == -1
    assert compare(2, 1) == 1

test "sort on mixed array":
    // NOTE the exact order is undefined, it depends on the kind pointers ... should perhaps fix that one day
    res = ["hello", 1, 2, 3, "world", {a=1},{b=1}].sort
    //print repr res
    assert res.find(1) + 1 == res.find(2)
    assert res.find(2) + 1 == res.find(3)
    assert res.find("hello") + 1 == res.find("world")

