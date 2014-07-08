test "basic":
    assert "hello".size == 5
    assert "hello"[1] == 'h'
    assert "hello"[5] == 'o'
    assert "hello"[-5] == 'h'
    assert "hello"[-1] == 'o'
    assert isUndefined("hello"[6])
    assert isUndefined("hello"[-6])
    assert "hello".hash
    assert "hello" == "hello"
    assert "hello" == "h" +"ello"

test "slice":
    assert "hello"[2:-2] == "ell"
    assert "hello"[2:4] == "ell"

test "iterate":
    "hello".each: l, n ->
        { n == 1 }: assert l == 'h'
        { n == 2 }: assert l == 'e'
        { n == 3 }: assert l == 'l'
        { n == 4 }: assert l == 'l'
        { n == 5 }: assert l == 'o'
        {        }: assert false

test "iterate fn":
    "hello".each(l, n -> assert n >= 1 and n <= 5)

test "map":
    assert "hello".map(l, n -> l) == ['h','e','l','l','o']
    assert "hello".map(l, n -> n) == [1,2,3,4,5]

test "from various constructors":
    assert String(Bin("hello")) == "hello"
    assert String(Bin("hel"), Buffer.new("lo")) == "hello"
    assert String(Buffer.new("hello")) == "hello"
    assert String(104,101,108,108,111) == "hello"
    assert String([104,101,108,108,111]) == "hello"
    assert String(Array.new([104,101,108,108,111])) == "hello"

test "from flattened list":
    assert String([[104,101],[108,108,[111]]]) == "hello"
