class = x -> {
    y = -> print x
}
print repr(class)
object = class(42)
print repr(object)
object.y
