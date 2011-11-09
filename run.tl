Point = [
    new: { x, y -> print "this:", this; [ x: x, y: y, class: this.class ].toObject }
    class: [
        area: { -> this.x * this.y }
    ].toObject
].toObject

p = Point.new 10, 10
p.area

