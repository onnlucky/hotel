test "map":
    map = HashMap.new
    map["a"] = "-1"
    map["b"] = "-2"
    ls = map.map(k, v, n -> k + v)
    assert ls.size == 2
    assert ls[1] == "a-1" or ls[2] == "b-2"

test "toMap":
    map = HashMap.new
    map["a"] = "-1"
    map["b"] = "-2"
    assert isMap(map.toMap)
    assert map.toMap["a"] == "-1"

test "toObject":
    map = HashMap.new
    map["a"] = "-1"
    map["b"] = "-2"
    assert isObject(map.toObject)
    assert map.toObject.a == "-1"
