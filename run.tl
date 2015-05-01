test = " assert eq(10 + 10, 20)

Vector2d = { this.x, this.y ->
    \"*\" = other, rhs ->
        other = { eq(this, other) }: rhs
                {                 }: other
        { eq(type(other), Vector2d) }: Vector2d(this.x * other.x - this.y * other.y, this.x * other.y - this.y * other.x)
        { isNumber(other)           }: Vector2d(this.x * other, this.y * other)
        {                           }: undefined
}

assert eq(Vector2d(10, 10), Vector2d(10, 10))
assert eq(Vector2d(10, 10), Vector2d(1, 1).*(10))

assert eq(Vector2d(10, 10) * 10, Vector2d(100, 100))
assert eq(10 * Vector2d(10, 10), Vector2d(100, 100))

print \"done\"
"

m = testcompiler.load(test, Env.current)
m.disasm
m.run

o = { "*" = 100 }
print repr o
print o."*"
print o.*
