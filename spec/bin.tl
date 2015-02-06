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

test "bin uses bytes in constructor, string uses chars":
    assert Bin(97) == "a"
    assert Bin(97) == String(97)
    assert Bin('a') == String('a')
    assert Bin(97) == Bin('a')

    assert Bin('€') == String(8364)
    # Bin constructor takes numbers as bytes, String sees numbers as chars (unicodepoints)
    assert Bin(8364) != String(8364)
    assert Bin(8364) != Bin('€')

test "bin/string/buffer constructors with io lists":
    l = 'a'
    assert Bin(l) == String(l)
    assert Bin(l) == Buffer.new(l).read
    assert String(l) == Buffer.new(l).read

    l2 = '€'
    assert l2 == Char(8364)
    assert l2 == "€"[1]

    assert Bin(l2) == String(l2)
    assert Bin(l2) == Buffer.new(l2).read
    assert String(l2) == Buffer.new(l2).read

    l3 = " hello"
    assert Bin(l, l2, l3) == Bin([l, l2], l3)
    assert Bin(l, l2, l3) == Bin([l, [l2, l3]])
    assert Bin(l, l2, l3) == Bin([l, [[l2], []]], [], l3)

    assert Bin(l, l2, l3) == String(l, l2, l3)
    assert Bin(l, l2, l3) == Buffer.new(l, l2, l3).read

    assert Bin([l, l2, l3]) == String([l, l2, l3])
    assert Bin([l, l2, l3]) == Buffer.new([l, l2, l3]).read

    assert Bin([l, [l2, l3]]) == String([l, [l2, l3]])
    assert Bin([l, [l2, l3]]) == Buffer.new([l, [l2, l3]]).read

    assert Bin([l, [[l2], []]], [], l3) == String([l, [[l2], []]], [], l3)
    assert Bin([l, [[l2], []]], [], l3) == Buffer.new([l, [[l2], []]], [], l3).read

