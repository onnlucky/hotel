in = "
assert 2 << 0 == 2
assert 2 << 1 == 4
assert 2 >> 1 == 1

assert 10 + 10 == 20 and not 10 + 10 != 20
assert 10 - 10 == 0
assert 10 * 10 == 100
assert 10 / 20 == 0.5
assert 10 // 10 == 1
assert 10 /. 20 == 0.5
assert 10 % 20 == 10
assert -10 % 20 == 10

assert 10.0 + 10 == 20
assert 10.0 * 10 == 100
assert 10.0 / 10 == 1
assert 10.0 % 20 == 10
assert -10.0 % 20 == 10

assert 2 ** 4 == 2 * 2 * 2 * 2
assert 10 < 11
assert 10 <= 11
assert 11 <= 11

Vector2d = { this.x, this.y ->
    \"*\" = other ->
        { other isa Vector2d }: Vector2d(this.x * other.x - this.y * other.y, this.x * other.y - this.y * other.x)
        { isNumber(other)    }: Vector2d(this.x * other, this.y * other)
        {                    }: undefined
}

assert Vector2d(10, 10) == Vector2d(10, 10)
assert Vector2d(10, 10) == Vector2d(1, 1).*(10)

assert Vector2d(10, 10) * 10 == Vector2d(100, 100)
assert 10 * Vector2d(10, 10) == Vector2d(100, 100)

assert \"hello\" * 3 == \"hellohellohello\"
assert 3 * \"hello\" == \"hellohellohello\"
assert 42 + \"hello\" == \"42hello\"
assert \"hello\" + 42 == \"hello42\"

print \"done\"
test: true
if true: print \"hello\"
"

m = testcompiler.load(in, _env)
m.run

