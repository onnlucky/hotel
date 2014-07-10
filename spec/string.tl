test "basic":
    assert "hello".size == 5
    assert "hello"[1] == 'h'
    assert "hello"[5] == 'o'
    assert "hello"[-5] == 'h'
    assert "hello"[-1] == 'o'
    assert isUndefined("hello"[6])
    assert isUndefined("hello"[-6])
    assert "hello".hash
    assert "hello".hash == ("he" + "llo").hash
    assert "hello" == "hello"
    assert "hello" == "h" +"ello"

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

test "reverse":
    assert "".reverse == ""
    assert "a".reverse == "a"
    assert "hello".reverse == "olleh"
    ref = "this is a very looooooooooooooooooooooooooong string that is veeeeeeeeeery long ..."
    assert ref.reverse.reverse == ref

test "upper":
    assert "hello".upper == "HELLO"

test "lower":
    assert "HELLO".lower == "hello"

test "slice":
    assert "hello"[1:-1] == "hello"
    assert "hello"[2:-2] == "ell"
    assert "hello"[2:4] == "ell"

    assert "hello"[1:0] == ""
    assert "hello"[1:1] == "h"
    assert "hello"[1:2] == "he"
    assert "hello"[1:3] == "hel"
    assert "hello"[1:4] == "hell"
    assert "hello"[1:5] == "hello"
    assert "hello"[1:6] == "hello"

    assert "hello"[1:-1] == "hello"
    assert "hello"[1:-2] == "hell"
    assert "hello"[1:-3] == "hel"
    assert "hello"[1:-4] == "he"
    assert "hello"[1:-5] == "h"
    assert "hello"[1:-6] == ""

    assert "hello"[0:0] == ""
    assert "hello"[0:1] == "h"
    assert "hello"[1:1] == "h"
    assert "hello"[2:1] == ""
    assert "hello"[3:1] == ""
    assert "hello"[10:1] == ""

