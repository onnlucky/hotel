test "construction":
    assert Bin(1,2,3,4).size == 4
    assert Bin(1,2,3,4)[1] == 1
    assert Bin(1,2,3,4)[4] == 4
    assert Bin(1,2,3,4)[-1] == 4
    assert Bin(1,2,3,4)[-4] == 1

test "out of bounds is undefined":
    assert not isDefined(Bin(1,2,3,4)[5])
    assert not isDefined(Bin(1,2,3,4)[-5])
    assert not isDefined(Bin(1,2,3,4)[100])
    assert not isDefined(Bin(1,2,3,4)[-100])

test "masks":
    assert Bin(256)[1] == 0
    assert Bin(-1)[1] == 255

test "from string":
    assert Bin("hello world")[6] == 32
    assert Bin("hello", " ", "world")[6] == 32

test "from bin":
    assert Bin(Bin("hello world"))[6] == 32
    assert Bin(Bin("hello"), Bin(" "), Bin("world"))[6] == 32

test "from buffer":
    buf = Buffer.new("hello world")
    assert Bin(Buffer.new("hello world"))[6] == 32
    assert Bin(Buffer.new("hello"), Buffer.new(" "), Buffer.new("world"))[6] == 32

test "from mixed":
    assert Bin("hello", 32, Buffer.new("world")) == Bin("hello world")

test "slice":
    assert Bin("hello")[2:-2] == Bin("hello"[2:-2])

test "bin equals string":
    assert Bin("hello") == "hello"
