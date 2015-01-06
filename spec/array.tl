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

test "splice":
    res = Array.new([1,2,3,4,5])
    //res[2:3] = ["hello","world"]
    res.splice(2,4,["hello","world"])
    print repr res.toList
    assert res[1] == 1
    assert res[2] == "hello"
    assert res[3] == "world"
    assert res[4] == 5

    res = Array.new([1,2,3,4,5])
    //res[2:3] = ["hello"]
    res.splice(2,4,["hello"])
    assert res[1] == 1
    assert res[2] == "hello"
    assert res[3] == 5

test "reverse":
    res = Array.new([1,2,3,4,5])
    res.reverse
    print res[1], res[2], res[3], res[4], res[5]
    assert res[1] == 5
    assert res[2] == 4
    assert res[3] == 3
    assert res[4] == 2
    assert res[5] == 1
    assert res[-1] == 1
    assert res[-2] == 2

test "reverse 2":
    res = Array.new
    res.reverse
    assert isUndefined(res[1])

    res.add(1)
    res.reverse
    assert res[1] == 1
    assert isUndefined(res[2])

    res.add(2)
    res.reverse
    assert res[1] == 2
    assert res[2] == 1
    assert isUndefined(res[3])

    res.add(3)
    res.reverse
    assert res[1] == 3
    assert res[2] == 1
    assert res[3] == 2
    assert isUndefined(res[4])

