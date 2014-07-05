test "simple":
    assert [1].find(1) == 1
    assert ["hello","world",42,"foo","bar"].find("world") == 2
    assert [1,1,1,1].find(1) == 1
test "empty":
    assert not [].find(1)
    assert not [].find("hello")
    assert not [].find(null)
    assert not [].find(false)
test "not found":
    assert not ["hello","world",42,"foo","bar"].find("worldly")
    assert [1,2,3].find(4) == null
test "falsy":
    assert [true,false,null].find(true) == 1
    assert [true,false,null].find(false) == 2
    assert [true,false,null].find(null) == 3
test "from":
    assert [1,1,1,1].find(1, 2) == 1
    assert [1,1,1,1].find(1, from=2) == 2
    assert [1,1,1,1].find(1, from=4) == 4
    assert [1,1,1,1].find(1, from=0) == 1
test "negative from":
    assert [1,1,1,1].find(1, from=-1) == 4
    assert [1,1,1,1].find(1, from=-5) == 1
test "function":
    assert not ["hello", "foo", "bar"].find(e -> e == "foo")
test "block":
    res = ["hello", "foo", "bar"].find: e -> e == "foo"
    assert not res
test "function element":
    called = Var.new false
    target = x -> $called = true
    assert ["hello", "foo", target].find(target) == 3
    assert not $called

test "array":
    test = Array.new([1,2,3,4,5,6,7,8,9,10,"hello"])
    assert test.find(1) == 1
    assert test.find(2) == 2
    assert test.find("hello") == 11

test "string":
    test = "12345678910hello"
    assert test.find("1") == 1
    assert test.find("2") == 2
    assert test.find("hello") == 12
test "string - not found":
    test = "12345678910hello"
    assert not test.find("99")
    assert test.find("99") == null
test "string - from":
    test = "12345678910hello"
    assert test.find("1", 2) == 10
    assert test.find("1", from=2) == 10
test "string - chars":
    test = "12345678910hello"
    assert test.find('1') == 1
    assert test.find('1', 2) == 10
    assert test.find('1', from=2) == 10

    assert test.find(49) == 1
    assert test.find(49, 2) == 10
    assert test.find(49, from=2) == 10

test "bin":
    test = Bin(1,2,3,4,5,6,7,8,9,10)
    assert test.find(1) == 1
    assert test.find(2) == 2
    assert not test.find(99)
    assert test.find(99) == null

