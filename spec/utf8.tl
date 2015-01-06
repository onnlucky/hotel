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
