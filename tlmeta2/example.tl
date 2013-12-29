#!/bin/env
# single line comments ...
(# nested multi (# line comments #) yay #)

Shape: { x, y ->
  methods: {
    move: dx, dy -> this::copy({ x: x + dx, y: y + dy })
    string: -> "$x,$y"
  }
}

Rectangle: { x, y, width, height ->
  inherit: Shape(x, y)
  methods: {
    area: -> width * height
    string: -> "R[$x,$y $width, $height]"
    outline: -> this
  }
}

Circle: { x, y, radius ->
  inherit: Shape(x, y)
  methods: {
    area: -> radius ^ 2 * PI
    string: -> "C[$x,$y, $radius]"
    outline: -> Rectangle(x - radius, y - radius, radius * 2, radius * 2)
  }
}

shapes: [Rectangle(0, 0, 50, 50), Circle(10, 10, 50)]
shapes.map(s -> s.move(10, 10)).each(s -> print(s))
inspect(shapes)

