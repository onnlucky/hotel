test "basic":
    r = Regex("hello|world")
    assert r.match("hello").main == "hello"
    assert r.match("world").main == "world"
    assert not r.match("foobar")

    assert Regex("[^ ]+").match("test more").main == "test"

test "type":
    r = Regex("hello|world")
    assert isRegex(r)

test "subexpressions":
    test = "hello [2014-01-01] world"
    m = Regex("\\[([^]]+)\\].(.*)").match(test)
    assert m[1] == "2014-01-01"
    assert m[2] == "world"
    b, e = m.group(1)
    assert b == 8
    assert e == 17
    assert test[b:e] == "2014-01-01"
    assert test[b:e] == m[1]

test "find":
    r = Regex("hello|world")
    assert r.find("hello") == 1
    assert r.find("world") == 1
    assert not r.find("foobar")

test "find sub":
    test = "hello [2014-01-01] world"
    b, e = Regex("\\[([^]]+)\\]").find(test)
    assert b == 7
    assert e == 18
    assert test[b:e] == "[2014-01-01]"

test "sub":
    test = "test [hello] hello foo"
    r = Regex("\\[([^]]+)\\].*(foo)")
    m = r.match(test)
    assert m[1] == "hello"
    assert m[2] == "foo"
    assert isUndefined(m[3])
    assert m.size == 2
    assert m.main == test[6:]

test "utf8":
    euro = Char(0x20AC).toString
    test = "a,b,c,d,"+ euro +",f,g"
    r = Regex(euro)
    assert r.match(test).main == euro

    r = Regex(euro +"+")
    assert r.match(test).main == euro

    r = Regex(",("+ euro +"*),")
    assert r.match(test)[1] == euro

test "match from":
    m = Regex("[a-z]+").match("  foo bar  ")
    assert m.main == "foo"
    m = Regex("[a-z]+").match("  foo bar  ", from=2)
    assert m.main == "foo"
    m = Regex("[a-z]+").match("  foo bar  ", from=3)
    assert m.main == "foo"
    m = Regex("[a-z]+").match("  foo bar  ", from=4)
    assert m.main == "oo"
    m = Regex("[a-z]+").match("  foo bar  ", from=5)
    assert m.main == "o"
    m = Regex("[a-z]+").match("  foo bar  ", from=6)
    assert m.main == "bar"
    m = Regex("[a-z]+").match("  foo bar  ", from=-6)
    assert m.main == "bar"
    m = Regex("[a-z]+").match("  foo bar  ", from=-100)
    assert m.main == "foo"

test "find from":
    b, e = Regex("[a-z]+").find("  foo bar  ")
    assert b == 3 and e == 5
    b, e = Regex("[a-z]+").find("  foo bar  ", from=2)
    assert b == 3 and e == 5
    b, e = Regex("[a-z]+").find("  foo bar  ", from=3)
    assert b == 3 and e == 5
    b, e = Regex("[a-z]+").find("  foo bar  ", from=4)
    assert b == 4 and e == 5
    b, e = Regex("[a-z]+").find("  foo bar  ", from=5)
    assert b == 5 and e == 5
    b, e = Regex("[a-z]+").find("  foo bar  ", from=6)
    assert b == 7 and e == 9
    b, e = Regex("[a-z]+").find("  foo bar  ", from=-6)
    assert b == 7 and e == 9
    b, e = Regex("[a-z]+").find("  foo bar  ", from=-100)
    assert b == 3 and e == 5
