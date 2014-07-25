test "basic":
    buf = Buffer.new("hello world")
    test "reading a buffer empties it":
        assert buf.read(5) == Bin("hello")
        assert buf.read(1) == Bin(" ")
        assert buf.read(5) == Bin("world")
        assert buf.size == 0
    test "rewinding undoes reading":
        assert buf.rewind.size == "hello world".size
        assert buf.read() == Bin("hello world")
    test "but writing discards rewindable bytes":
        assert buf.size == 0
        buf.write("1")
        assert buf.rewind.size == 1
    test "isBuffer":
        assert isBuffer(buf)

test "find":
    buf = Buffer.new("hello world")
    assert buf.find("world") == 7
    assert buf.find(Bin("world")) == 7
    assert buf.find(Buffer.new("world")) == 7

    // bytes are 0-255 by forcing int and masking bits
    assert buf.find('w') == 7
    assert buf.find(119) == 7
    assert buf.find(119.0) == 7
    assert buf.find(119.0 + 256) == 7
    assert buf.find(119.0 - 256) == 7
    assert buf.find(119 - 256 - 256 - 256) == 7

test "find from and upto":
    buf = Buffer.new("1234567890 - 1234567890")
    assert buf.find("1", 11) == 14
    assert not buf.find("1", 11, 13)
    assert buf.find("1", null) == 1
    assert buf.find("1", 11, null) == 14
    assert buf.find("1", 11, 10000) == 14

test "favor from= and upto= keyword args":
    buf = Buffer.new("1234567890 - 1234567890")
    assert buf.find("1", from=11) == 14
    assert not buf.find("0", upto=5)
    assert not buf.find("1", from=11, upto=13)
    assert buf.find("1", from=null) == 1
    assert buf.find("1", from=11, upto=null) == 14

test "exception with bad from":
    buf = Buffer.new("1234567890 - 1234567890")
    assert try(buf.find("1", from="1")) == null
    assert try(buf.find("1", upto="1")) == null

test "boundaries of from":
    buf = Buffer.new("hello aaa")
    -10.to(20): n ->
        r = buf.find("aaa", from=n)
        // "aaa" is located at 7 and from is inclusive, so -2 and -1 fail
        { n == -2 }: assert not r
        { n == -1 }: assert not r
        { n <= 7  }: assert r
        {         }: assert not r

test "boundaries of upto":
    buf = Buffer.new("hello aaa")
    -10.to(20): n ->
        r = buf.find("aaa", upto=n)
        // upto is inclusive (like slice) so -1 and 9 are good
        { n == -1 }: assert r
        { n >= 9  }: assert r
        {         }: assert not r

