test "euro":
    s = String(Bin(0xE2, 0x82, 0xAC))
    assert s.bytes == 3
    assert s.size == 1
    assert s[1] == 0x20AC

test "euro char":
    l = Char(0x20AC)
    print l
    s = String(l)
    assert s.bytes == 3
    assert s.size == 1
    assert s[1] == 0x20AC

test "euro string":
    s = "€€"
    assert s.bytes == 6
    assert s.size == 2
    assert s[1] == 0x20AC
    assert s[2] == 0x20AC
