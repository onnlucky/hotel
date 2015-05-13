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

test "get with weird index":
    # TODO maybe remove this?
    assert "hello".get == 'h'
    assert "hello".get() == 'h'
    assert "hello".get(null) == 'h'

    assert isUndefined("hello"[0])
    #assert isUndefined("hello"[null])
    assert isUndefined("hello"["1"])
    assert isUndefined("hello"[10000])
    assert isUndefined("hello"[-10000])

test "iterate":
    var $called = false
    "hello".each: l, n ->
        $called = true
        { n == 1 }: assert l == 'h'
        { n == 2 }: assert l == 'e'
        { n == 3 }: assert l == 'l'
        { n == 4 }: assert l == 'l'
        { n == 5 }: assert l == 'o'
        {        }: assert false
    assert $called

test "iterate fn":
    "hello".each(l, n -> assert n >= 1 and n <= 5)

test "iterate external function":
    test = l, n ->
        assert l == 'x'
        assert not n
    "xxx".each(test)

test "map":
    assert "hello".map(l, n -> l) == ['h','e','l','l','o']
    assert "hello".map(l, n -> n) == [1,2,3,4,5]

test "map external function":
    test = l, n -> n
    assert "hello".map(test) == [null,null,null,null,null]

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

test "split":
    assert "hello world".split == ["hello","world"]
    assert "a, b, , d, e".split(",",trim=true) == ["a","b","","d","e"]
    assert "a, b, , d, e".split(",",trim=true,keepempty=false) == ["a","b","d","e"]
    assert ", b, , d, ".split(",",trim=true,keepempty=true) == ["","b","","d",""]

test "split regex":
    assert "foobar".split(Regex("[aeiou]+")) == ["f","b","r"]

test "split empty":
    assert "".split("\n") == [""]

test "find":
    assert "12345a789a12345".find('a') == 6
    assert "12345a789a12345".find("a") == 6
    assert "12345a789a12345".find("a7") == 6
    assert "12345a789a12345".find("a1") == 10

test "find from":
    assert "12345a789a12345".find('a', from=6) == 6
    assert "12345a789a12345".find('a', from=7) == 10
    assert "12345a789a12345".find("a", from=6) == 6
    assert "12345a789a12345".find("a", from=7) == 10
    assert "12345a789a12345".find("a7", from=7) == null

test "find backward":
    assert "12345a789a12345".find('a', backward=true) == 10
    assert "12345a789a12345".find("a", backward=true) == 10

test "find backward from":
    assert "12345a789a12345".find('a', from=9, backward=true) == 6
    assert "12345a789a12345".find("a", from=9, backward=true) == 6
    assert "12345a789a12345".find('a', from=10, backward=true) == 10
    assert "12345a789a12345".find("a", from=10, backward=true) == 10
    assert "12345a789a12345".find("a7", from=10, backward=true) == 6

test "find backward upto":
    assert "12345a789a12345".find('a', from=9, upto=7, backward=true) == null
    assert "12345a789a12345".find("a", from=9, upto=7, backward=true) == null
    assert "12345a789a12345".find('a', from=9, upto=6, backward=true) == 6
    assert "12345a789a12345".find("a", from=9, upto=6, backward=true) == 6

test "startsWith":
    assert "hello".startsWith("hello")
    assert "hello".startsWith("hell")
    assert "hello".startsWith("hel")

    assert "hello".startsWith("ello", 2)

test "url decode":
    assert _urldecode("foo%2Fbar") == "foo/bar"
    assert _urldecode("foo+bar") == "foo+bar"
    assert _urldecode("%") == ""
    assert _urldecode("%22Aardvark-._~s%20lurk%2C%20OK%3F%22") == "\"Aardvark-._~s lurk, OK?\""

test "url encode":
    assert _urlencode("foo/bar") == "foo%2Fbar"
    assert _urlencode("foo+bar") == "foo%2Bbar"
    assert _urlencode("\"Aardvark-._~s lurk, OK?\"") == "%22Aardvark-._~s%20lurk%2C%20OK%3F%22"
    assert _urlencode("//////////") == "%2F%2F%2F%2F%2F%2F%2F%2F%2F%2F"

    x = "!!!foo bar/?&&&%%%%"
    assert _urldecode(_urlencode(x)) == x

