test "euro":
    s = String(Bin(0xE2, 0x82, 0xAC))
    assert s.bytes == 3
    assert s.size == 1
    assert s[1] == 0x20AC

test "euro char":
    l = Char(0x20AC)
    s = String(l)
    assert s.bytes == 3
    assert s.size == 1
    assert s[1] == 0x20AC

test "euro string":
    s = "€€ hello €€ world €€"
    assert s.size == 20
    assert s.bytes == 20 + 6 * 2
    assert s[1] == 0x20AC
    assert s[2] == 0x20AC
    assert s[-1] == 0x20AC
    assert s[-2] == 0x20AC

    assert s[4:8] == "hello"
    assert s[10:11] == "€€"
    assert s[13:17] == "world"
    assert s[-8:-4] == "world"
    assert s[-3:] == " €€"

test "reverse":
    "€€ hello €€ world €€".reverse[10:11] == "€€"
    assert "h€llø".reverse == "øll€h"

test "find":
    s = "€€ hello €€"
    assert s.size == 11
    assert s.bytes == 11 + 4 * 2
    assert s[1] == 0x20AC
    assert s.find('h')
    assert s.find(104) // 'h'
    assert s.find(104) == 4
    assert s.find(0x20AC) == 1
    assert s.find(104, backward=true)
    assert s.find(104, backward=true) == 4
    assert s.find(0x20AC, backward=true) == 11

test "find 2":
    s = "hello €€€€ world"
    assert s.find("hello") == 1
    assert s.find("€€€€") == 7
    assert s.find("world") == 12

    assert s.find("hello", backward=true) == 1
    assert s.find("€€€€", backward=true) == 7
    assert s.find("world", backward=true) == 12

test "regex":
    r = Regex("hello|world|€€€")
    b, e = r.find("€€€")
    assert b == 1 and e == 3
    b, e = r.find("€oo €€€ world")
    assert b == 5 and e == 7
    b, e = r.find("€oo €€€ world", from=6)
    assert b == 9 and e == 13

test "regex groups":
    r = Regex("(hello|world|€€€)")
    m = r.match("€oo €€€ world", from=6)
    b, e = m.group(1)
    assert b == 9 and e == 13
    assert m.main == "world"

