test "addAll":
    res = Array.new
    res.addAll([1,2,3,4])
    assert res.size == 4
    assert res[1] == 1
    assert res[-1] == 4
