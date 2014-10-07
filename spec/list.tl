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
    var $called = false
    test = e, n ->
        $called = true
        assert not n
        assert e >= 1 and e <= 5
    [1,2,3,4,5].each(test)
    assert $called

test "break":
    var $count = 0
    [1,2,3,4,5].each(e ->
        $count += e
        if e == 3: break
    )
    assert $count == 6

test "continue":
    var $count = 0
    [1,2,3,4,5].each(e ->
        if e > 3: continue
        $count += e
    )
    assert $count == 6

test "reverse":
    assert [1,2,3,4,5].reverse == [5,4,3,2,1]

test "flatten":
    assert [[1,2],3,[4,5]].flatten == [1,2,3,4,5]

test "map":
    assert [1,2].map(n -> n * 3) == [3,6]

test "reduce":
    assert isUndefined([].reduce(tmp, e -> tmp + e))
    assert [].reduce(initial=0, (tmp, e -> tmp + e)) == 0
    assert [1].reduce(tmp, e -> tmp + e) == 1
    assert [1].reduce(initial=0, (tmp, e -> tmp + e)) == 1
    assert [1,2,3].reduce(tmp, e -> tmp + e) == 6
    assert [1,2,3].reduce(initial=6, (tmp, e -> tmp + e)) == 12
    res = [1,2,3].reduce: tmp, e -> tmp + e
    assert res == 6
    res = [1,2,3].reduce(initial=6): tmp, e -> tmp + e
    assert res == 12

    assert isUndefined([].reduce(tmp, e -> 42))
    assert [1].reduce(tmp, e -> 42) == 1
    assert [1,2].reduce(tmp, e -> 42) == 42

    assert [].reduce(initial=42, (tmp, e -> tmp)) == 42
    assert [1].reduce(initial=42, (tmp, e -> tmp)) == 42
    assert [1,2,3].reduce(initial=42, (tmp, e -> tmp)) == 42

