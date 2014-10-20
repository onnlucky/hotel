fn = (
    fn = -> x = 10
)

var $x = 42
x = 100
print x, $x
x = 100
fn()
print x, $x

