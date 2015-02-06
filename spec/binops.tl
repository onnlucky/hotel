test "and/or/xor/not":
    x = 0b1011_0000
    y = 0b1111_0100
    assert x & y == 0b1011_0000
    assert x | y == 0b1111_0100
    assert x ^ y == 0b0100_0100
    assert ~x & 0xFF == 0b0100_1111

test "not":
    s = ~7.toString(2)
    assert s[1] == '1'
    assert s[32] == '0'
    assert s[31] == '0'
    assert s[30] == '0'
    assert s[29] == '1'

test "binary operators return unsigned 32 bit values":
    assert ~7 > 0
    assert ~-7 > 0

    assert -5 & 0xFF == 0b1111_1011
    assert -5 & 0xFF == 251
    assert -5 & 0xFF == 2**8 - 5
    assert -5 & 0xFFFF == 2**16 - 5
    assert -5 & 0xFF_FFFF == 2**24 - 5
    assert -5 & 0xFFFF_FFFF == 2**32 - 5
    assert -5 & 0x1_FFFF_FFFF == 2**32 - 5
    assert -5 & 0xFFFF_FFFF_FFFF == 2**32 - 5

    assert -5 | 0xFF == 0xFFFF_FFFF
    assert -5 | 0xFFFF == 0xFFFF_FFFF
    assert -5 | 0xFFFF_FFFF == 0xFFFF_FFFF
    assert -5 | 0xFFFF_FFFF_FFFF == 0xFFFF_FFFF

    assert -5 ^ 0xFF == 0xFFFF_FF04

test "negation":
    x = 4
    assert(~x == (-4 - 1) & 0xFFFF_FFFF)
    assert((~x) == (-4 - 1) & 0xFFFF_FFFF)
    assert(-x == -4)
    assert((-x) == -4)

# shifting works on arbitrary precision numbers
test "basic shifting":
    assert 1 >> 0 == 1
    assert 1 >> 1 == 0
    assert 1 << 0 == 1
    assert 1 << 0 == 2**0
    assert 1 << 1 == 2**1
    assert 1 << 2 == 2**2
    assert 1 << 3 == 2**3

test "negative shift is the reverse":
    assert 8 >> -1 == 16
    assert 8 >> -1 == 8 << 1
    assert 8 >> -2 == 32
    assert 8 >> -2 == 8 << 2
    assert 8 << -1 == 4
    assert 8 << -1 == 8 >> 1
    assert 8 << -2 == 2
    assert 8 << -2 == 8 >> 2

test "negative shifting":
    assert -1 >> 0 == -1
    assert -1 >> 1 == -1
    assert -1 >> 2 == -1
    assert -1 >> 3 == -1

    assert -1 << 0 == -1
    assert -1 << 0 == -(2**0)
    assert -1 << 1 == -(2**1)
    assert -1 << 2 == -(2**2)
    assert -1 << 3 == -(2**3)

test "left shift large":
    assert 1 << 30 == 2**30
    assert 1 << 31 == 2**31
    assert 1 << 32 == 2**32
    assert 1 << 33 == 2**33
    assert 1 << 60 == 2**60
    #assert 1 << 70 == (2**70) # TODO arbitrary precision
    assert -1 << 30 == -(2**30)
    assert -1 << 60 == -(2**60)
    #assert -1 << 70 == -(2**70) # TODO arbitrary precision

test "right shift large":
    assert -16 >> 2 == -4
    assert -16 << -2 == -4
    assert -65535 >> 2 == -(2**14)
    assert -65535 >> 8 == -(2**8)
    assert 2**60 >> 50 == 2**10
    assert -(2**60) >> 50 == -(2**10)
    #assert 2**70 >> 50 == 2**20
    #assert -(2**70) >> 50 == -(2**20)

# except for unsigned right shift, it works on 32 bit unsigned numbers, and thus shifts in zeros
test "unsigned right shift":
    assert 16 >>> 2 == 16 >> 2
    assert -16 >>> 2 == (-16 & 0xFFFF_FFFF) >> 2
    assert -1 >>> 0 == 4294967295
    assert -1745539 >>> 15 == 131018
    assert 1745539 >>> 15 == 53
    assert 2 ** 33 >>> 1 == 0

