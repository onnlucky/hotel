test "toString(base)":
    assert 999.toString() == "999"
    assert 999.toString(16) == "3e7"
    assert 999.toString(2) == "1111100111"

    assert Number(999.toString()) == 999
    assert Number("0x"+ 999.toString(16)) == 999
    assert Number("0b"+ 999.toString(2)) == 999

test "large numbers":
    x = 8543688244379927653322881
    assert Number(x.toString()) == x
    assert Number("0x"+ x.toString(16)) == x
    assert Number("0b"+ x.toString(2)) == x

test "stay int dispite operations":
    big = 8543688244379927653322881
    assert isInt(10 /. 2)
    assert isBignum(big /. 2)
    assert isInt(2^31)
    assert isBignum(2^64)

test "float stays float":
    big = 8543688244379927653322881
    assert isFloat(10 / 2)
    assert isFloat(big / 2)
    assert isFloat(10.0 * 256)
    assert isFloat(2.0^64)
