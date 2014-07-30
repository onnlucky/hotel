test "basic strftime example":
    assert strftime("%c", 1406703361) == "Wed Jul 30 08:56:01 2014"

test "strptime":
    test = "23:55:03.684"
    tm, chars = strptime("%H:%M:%S.", test)
    assert tm.hour == 23
    assert tm.min == 55
    assert tm.sec == 03
    assert tm.isdst == 0
    assert test[:chars] == strftime("%H:%M:%S.", tm)
    assert test[chars + 1:] == "684"

