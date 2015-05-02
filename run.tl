in = "
assert 10 + 10 == 20
assert 10 - 10 == 0
assert 10 * 10 == 100
assert 10 / 20 == 0.5
assert 10 // 10 == 1
assert 10 /. 20 == 0.5

#assert 10.0 + 10 == 20

Vector2d = { this.x, this.y ->
    \"*\" = other, rhs ->
        other = { this == other }: rhs
                {               }: other
        { other isa Vector2d }: Vector2d(this.x * other.x - this.y * other.y, this.x * other.y - this.y * other.x)
        { isNumber(other)    }: Vector2d(this.x * other, this.y * other)
        {                    }: undefined
}

assert Vector2d(10, 10) == Vector2d(10, 10)
assert Vector2d(10, 10) == Vector2d(1, 1).*(10)

assert Vector2d(10, 10) * 10 == Vector2d(100, 100)
assert 10 * Vector2d(10, 10) == Vector2d(100, 100)

print \"done\"
test: true
"

m = testcompiler.load(in, _env)
m.run

