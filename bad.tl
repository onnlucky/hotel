/*
class = { x ->
    y = -> print z, x
    z = 100
}
*/
/*
class = x -> {
    z = 100
    y = -> print z, x
}
*/
_class = { y = this -> print this.z, this.x }
class = x -> { class = _class, x = x, z = 100 }

print repr(class)
object = class(42)
print repr(object)
object.y

