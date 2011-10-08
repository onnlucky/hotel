foo = { y = 20 }
print foo
{ x = 10, class = foo }.toObject.y

/*
SquareClass = {
    area = ->{ this.x * this.y }
}
Square = {
    class = SquareClass
    x = 10
    y = 10
}
Square.area

/*
list = _new_object({
    _set_= (v)->{ _this_set(#value, v) }
    _get_=    ->{ _this_get(#value) }
})

list._set_ 10
list._set_ 20
list._set_ 30
list._set_ 40
print list._get_

