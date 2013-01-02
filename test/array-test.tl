ls = Array.new
ls.insert(10, "hello world!")
assert ls.get(9) == null
assert ls.get(10) == "hello world!"
assert isUndefined ls.get(11)

while ls.size > 0: ls.pop

x = Var.new 10
while $x > 0: {
    $x -= 1
    ls.insert(random(1000), "hello oeps");
}
ls.insert(1000, "auch")
ls.insert(500, "hello")
ls.insert(3, "world")

x = Var.new 10
while $x > 0: {
    $x -= 1
    ls.remove(10 + $x * 5);
}

assert ls.get(491) == "hello"
assert ls.get(3) == "world"
assert ls.remove(3) == "world"
assert ls.remove(490) == "hello"

seen_auch = Var.new false
while ls.size > 0: {
    v = ls.pop
    if v == "auch": $seen_auch = true
    assert not v == "hello"
    assert not v == "world"
    assert isDefined v
}
assert $seen_auch

assert isUndefined ls.pop
assert isUndefined ls.get(-100)
assert isUndefined ls.get(0)
assert isUndefined ls.get(1)
assert isUndefined ls.get(10000)

