test "basic":
    ref = Set(1,2,3,4,5,"hello","world")
    assert ref(1)
    assert ref.size == 7
    assert not ref(6)
    assert ref.has(1)
    assert ref("world")
    assert ref.has("world")
    assert not ref("foo")
    assert not ref.has("foo")

test "get":
    ref = Set(1,2,3,4,5,"hello","world")
    assert ref(ref[1])
    assert ref(ref[6])
    assert ref[1] == 1 or ref[1] == "hello" or ref[1] == 5 or ref[1] == "world"

test "iterate":
    ref = Set(1,2,3,4,5,"hello","world")
    ref.each: e, n ->
        assert n >= 1 and n <= 7
        assert ref(e)
        assert ref.has(e)

test "slice":
    ref = Set(1,2,3,4,5,"hello","world")
    assert ref[1:2].size == 2
