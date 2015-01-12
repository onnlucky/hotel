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

    assert [1,2,3].first == 1
    assert [1,2,3].last == 3

test "slice":
    assert [1,2,3,4,5][2:-2] == [2,3,4]
    assert [1,2,3,4,5][2:4] == [2,3,4]
    assert [1,2,3,4,5][1:-1] == [1,2,3,4,5]
    assert [1,2,3,4,5][1:0] == []
    assert [1,2,3,4,5][1:1] == [1]
    assert [1,2,3,4,5][1:2] == [1,2]
    assert [1,2,3,4,5][1:100] == [1,2,3,4,5]

test "invalid index":
    assert isUndefined([1][null])
    assert isUndefined([1]["hello"])
    assert isUndefined([1]["1"])
    assert isUndefined([1][0])
    assert isUndefined([1][2])
    assert isUndefined([1][-2])

test "change":
    assert [1,2].add(3) == [1,2,3]
    assert [2,3].prepend(1) == [1,2,3]
    assert [2,3].prepend(1) == [1,2,3]
    assert [1,9,3].set(2, 2) == [1,2,3]
    assert [1,9,3].set(4, 2) == [1,9,3,2]
    assert [1,9,3].set(5, 2) == [1,9,3,null,2]
    assert [1,9,3].set(6, 2) == [1,9,3,null,null,2]
    assert [1,2,3].remove(1) == [2,3]
    assert [1,2,3].remove(2) == [1,3]
    assert [1,2,3].remove(3) == [1,2]
    assert [1,2,3].remove(4) == [1,2,3]

test "comparing":
    assert [1,2,3] < [2,3,4]
    assert [1,2] < [2,3,4]
    assert [1,2] > [-1, 2,3,4]

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

