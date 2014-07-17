// hotel cannot parse utf8 source code (correctly) yet

test "euro":
    s = String(Bin(0xE2, 0x82, 0xAC))
    assert s.size == 1
    assert s.bytes == 3

test "euro char":
    l = Char(0x20AC)
    print l
    s = String(l)
    assert s.size == 1
    assert s.bytes == 3
