test = "hello world"
assert _inflate(_deflate(bin("hello world"), 9)).toString == "hello world"
assert _inflate(_deflate(bin("hello world"), 9, true), true).toString == "hello world"

data = io.File("/usr/share/dict/words").read
assert data.size > _deflate(data).size
assert data.size > _deflate(data, -1, true).size
