(# PROPOSAL:
Point = class {x, y ->
    move = dx, dy -> this.{ x = x + dx, y = y + dy }
}
Square = class {x, y, width, height ->
    inherit Point(x, y)
    area = -> width * height
    static Null = Square(0, 0, 0, 0)
}
#)
# DESUGARS TO:
Point = {
    call = x, y -> { x = x, y = y, class = this.class }
    class = {
        move = dx, dy -> Object.set(this, { x = this.x + dx, y = this.y + dy })
    }
}
Square = {
    call = x, y, width, height ->
        _1 = Point(x, y)
        Object.inherit({width = width, height = height, class = this.class}, _1)
    class = {
        area = -> this.width * this.height
        class = Point.class
    }
    #Null = (->Square(0, 0, 0, 0)).lazy
}
s1 = Square(10, 20, 42, 10)
out s1.x, " ", s1.y, " ", s1.width, " ", s1.height, " area: ", s1.area, "\n"
s2 = s1.move(100, 100)
out s2.x, " ", s2.y, " ", s2.area, "\n"

