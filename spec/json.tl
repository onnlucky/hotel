test "parse":
    assert json.parse("{hello:42}").hello == 42

test "toString":
    assert json.toString({hello=42}) == "{\"hello\":42}"

test "toBin":
    assert json.toBin({hello=42}) == Bin("{\"hello\":42}")

test "toBuffer":
    assert json.toBuffer({hello=42}).readString == "{\"hello\":42}"

test "map":
    assert isMap([hello=42])
    assert not isObject([hello=42])
    assert json.toString([hello=42]) == "{\"hello\":42}"

test "set":
    assert json.toString(Set("hello")) == "{\"hello\":true}"

test "array":
    assert json.toString(Array.new([1,2,3])) == "[1,2,3]"

test "hashmap":
    assert json.toString(HashMap.new({hello=42})) == "{\"hello\":42}"

test "complex":
    a = [1,2,3,4,5,6,7,8,9,0]
    b = ["hello","world"]
    c = [3.14, 2.71]
    d = [true,false,null]
    ref = {{a,b,c,d}}
    assert json.parse(json.toString(ref)) == ref

test "floats":
    ref = [3.14159265359,2.71828182846]
    assert json.parse(json.toString(ref)) == ref

test "circular":
    a = HashMap.new({hello=42})
    a.set("test", true)
    a.set("self", a)
    assert a["test"]
    assert a["self"]
    assert not a["foobar"]
    json.toString(a)

