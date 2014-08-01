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

test "times":
    res = Array.new
    10.times: n ->
        if n == 8: break
        if n == 3: continue
        res.add(n)
    assert res.size == 6
    assert res[1] == 1
    assert res[6] == 7

test "upto":
    res = Array.new
    10.to(50): n ->
        r = { n % 3 == 0 and n % 5 == 0 }: "fizz buzz"
            { n % 3 == 0                }: "fizz"
            { n % 5 == 0                }: "buzz"
            {                           }: continue
        res.add(r)
    assert res[1] == "buzz" // 10
    assert res[2] == "fizz" // 12
    assert res[3] == "fizz buzz" // 15
    assert res[-1] == "buzz" // 50
    assert res.size < 20
