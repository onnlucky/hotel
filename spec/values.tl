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
    assert compare(1, 2) == -1
    assert compare(2, 1) == 1
    assert compare(null, 1) == -1 // smaller
    assert compare(1, "foo") == -1 // smaller
    assert compare("foo", 1) == 1
    assert compare("foo", "fo"+"o") == 0

test "sort on mixed array":
    // NOTE the exact order is undefined, it depends on the kind pointers ... should perhaps fix that one day
    res = ["hello", 1, 2, 3, "world", {a=1},{b=1}].sort
    //print repr res
    assert res.find(1) + 1 == res.find(2)
    assert res.find(2) + 1 == res.find(3)
    assert res.find("hello") + 1 == res.find("world")

test "max":
    assert [1,2,3,4].max == 4
    assert ["hello","world","zzz"].max == "zzz"
    assert [].max(42) == 42
    assert not isDefined([].max)

test "max on mixed array":
    res = ["hello", 4, 1, 2, 3, "zzz", "world"].max
    assert res == "zzz"

test "min":
    assert [1,2,3,4].min == 1
    assert ["hello","world","zzz"].min == "hello"

test "min on mixed array":
    res = ["hello", 4, 1, 2, 3, "zzz", "world"].min
    assert res == 1

test "number constructor":
    assert Number(1) == 1
    assert Number("1") == 1
    assert Number("001") == 1
    assert Number("0_1") == 1
    assert Number("0x1") == 1
    assert Number("0b1") == 1
    assert Number("12345678901234567890") == 12345678901234567890

test "number from char returns number only for 0-9":
    assert Number('1') == 1
    assert Number("12345"[1]) == 1
    assert Number(' ') == null
    assert Number('a') == null

test "number from bad things return null":
    assert Number("") == null
    assert Number("a") == null
    assert Number('a') == null
    assert Number("1,2,3") == null
    assert Number([1]) == null

