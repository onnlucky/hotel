assert 1 < 2, 1 <= 2, 1 <= 1, 1 == 1
assert 2 > 1, 2 >= 1, 2 >= 2, 2 == 2

STRING = "12345"
assert STRING[0:] == STRING
assert STRING[:0] == ""
assert STRING[STRING.size:] == "5"
assert STRING[:STRING.size] == STRING

LIST = [1, 2, 3, 4, 5]
assert LIST[0:] == LIST
assert LIST[:0] == []
assert LIST[LIST.size:] == [5]
assert LIST[:LIST.size] == LIST

assert #hello == "hello"
assert #hello.hash == "hello".hash
assert #hello[1:-1] == "hello"[1:-1]

assert "1X3".replace("X", "2") == "123"

assert ["1", "2", "3"].join == "123"
sb = Array.new
sb.add("1"); sb.add("2"); sb.add("3")
assert sb.join == "123"
