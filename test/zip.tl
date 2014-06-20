test = "hello world"
assert zlib.inflate(zlib.deflate(bin("hello world"), 9)).toString == "hello world"
assert zlib.inflate(zlib.deflate(bin("hello world"), 9, true), true).toString == "hello world"

data = io.File("/usr/share/dict/words").read
assert data.size > zlib.deflate(data).size
assert data.size > zlib.deflate(data, -1, true).size
