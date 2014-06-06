class = { x ->
    z = 42
    y = -> print x
}
// class = { y = -> this=args["this"]; print this.x }
// constructor = x -> {{x,class}}

