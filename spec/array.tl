test "addAll":
    res = Array.new
    res.addAll([1,2,3,4])
    assert res.size == 4
    assert res[1] == 1
    assert res[-1] == 4

test "negative index":
    res = Array.new([1,2,3,4])
    assert res[-1] == 4
    assert res[4] == 4
    res[-1] = "hello"
    assert res[4] == "hello"
    res.remove(-2)
    assert res[3] == "hello"
    res.remove(2)
    assert res[2] == "hello"

    res.insert(-1, "oeps")
    assert res[3] == "hello"
    res.insert(-1, "oeps")
    assert res[4] == "hello"

test "sparse":
    res = Array.new([1,2,3,4])
    res[100] = "hello"
    assert res[4] == 4
    assert res[5] == null
    assert res[100] == "hello"
    assert res.size == 100
