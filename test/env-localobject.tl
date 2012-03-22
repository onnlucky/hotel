Point = (
    class = {
        distance: -> sqrt(this._x * this._x + this._y * this._y)
    }
    call = { _x, _y ->
        class = class
        Env.localObject
    }
    zero = call(0, 0)
    Env.localObject
)
print Point(10, 10).distance
print Point.zero.distance

