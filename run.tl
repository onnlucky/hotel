Vector2d = { this.x, this.y ->
    mul = l, r ->
        { l isa Vector2d and r isa Vector2d }: Vector2d(l.x * r.x - l.y * b.y, l.x * r.y - l.y * b.x)
        { l isa Vector2d and isNumber(r)    }: Vector2d(l.x * r, l.y * r)
        { isNumber(l) and r isa Vector2d    }: Vector2d(r.x * l, r.y * l)
        {                                   }: undefined
}

test "value class equality":
    assert Vector2d(10, 10) == Vector2d(10, 10)
    assert Vector2d(10, 10) == Vector2d(1, 1).mul(10)

test "reimplement operators":
    assert Vector2d(10, 10) * 10 == Vector2d(100, 100)

