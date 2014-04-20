Point = (
    class = {
        distance = -> sqrt(this._x * this._x + this._y * this._y)
    }
    call = _x, _y ->
        class = class
        Env.localObject
    zero = call(0, 0)
    Env.localObject
)
// perhaps not a good idea to test sqrt ...
print Point(10, 10).distance.round
print Point.zero.distance.round

