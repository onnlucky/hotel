test "basic":
    r = Regex("hello|world")
    assert r.match("hello").main == "hello"
    assert r.match("world").main == "world"
    assert not r.match("foobar")

    assert Regex("[^ ]+").match("test more").main == "test"

test "subexpressions":
    m = Regex("\\[([^]]+)\\]\s*(.*)").match("[2014-01-01] hello world")
    m[1] == "2014-01-01"
    m[2] == "hello world"

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
