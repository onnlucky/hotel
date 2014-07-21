test "basics":
    assert [1,2,3,4,5].size == 5
    assert [1,2,3,4,5][1] == 1
    assert [1,2,3,4,5][5] == 5
    assert [1,2,3,4,5][-5] == 1
    assert [1,2,3,4,5][-1] == 5
    assert isUndefined([1,2,3,4,5][6])
    assert isUndefined([1,2,3,4,5][-6])
    assert [1,2,3,4,5].hash
    assert [1,2,3,4,5].hash == ([1,2] + [3,4,5]).hash
    assert [1,2,3,4,5] == [1,2,3,4,5]
    assert [1,2,3,4,5] == [1] + [2,3,4,5]

test "slice":
    assert [1,2,3,4,5][2:-2] == [2,3,4]
    assert [1,2,3,4,5][2:4] == [2,3,4]
    assert [1,2,3,4,5][1:-1] == [1,2,3,4,5]
    assert [1,2,3,4,5][1:0] == []
    assert [1,2,3,4,5][1:1] == [1]
    assert [1,2,3,4,5][1:2] == [1,2]
    assert [1,2,3,4,5][1:100] == [1,2,3,4,5]

test "iterate":
    [1,2,3,4,5].each: e, n -> assert e == n

test "iterate fn":
    [1,2,3,4,5].each(e, n -> assert e == n)

test "iterate external function":
    called = Var.new false
    test = e, n ->
        $called = true
        assert not n
        assert e >= 1 and e <= 5
    [1,2,3,4,5].each(test)
    assert $called

test "reverse":
    assert [1,2,3,4,5].reverse == [5,4,3,2,1]

test "flatten":
    assert [[1,2],3,[4,5]].flatten == [1,2,3,4,5]

test "map":
    assert [1,2].map(n -> n * 3) == [3,6]

