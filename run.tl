fatal = ->{ FATAL }
if = (cond, &tb, &fb)->{ goto(bool(cond, tb, fb)()) }
while = (&cond, body)->{
    again = continuation
    if cond(), (body(); again())
}
or  = (left, &right)->{ bool(left, ->{left}, right)() }
and = (left, &right)->{ bool(left, right, ->{left})() }
xor = (left,  right)->{ bool(left, bool(right, false, true), bool(right, true, false)) }

Text = {
    size = ->{ _text_size(this) }
    slice = (begin, end)->{ _text_slice(this, begin, end) }
}
Map = {
    size = ->{ _map_size(this) }
    get = (k)->{ _map_get(this, k) }
}

_host_classes = [
    0, 1, 2, 3, 4, 5, 6, 7, 8, Text, 10, 11, Map, 13, 14, 15, 16, 17, 18, 19, 20
]

_object_send2 = (object, class, name, args)->{
    field = _map_get(class, name)
    if field != undefined, (
        if _callable_is(field), goto(_method_invoke(field, args))
        return field
    )
    class = _map_get(class, #class)
    if class, goto(_object_send2(object, class, name, args))
    return undefined
}

_object_send = (object, name)->{
    if _object_is(object), goto(_object_send2(object, object, name, args))

    class = _list_get(_host_classes, _value_type(object))
    field = _map_get(class, name)
    print("class:", class, "field:", field)
    if _callable_is(field), goto(_method_invoke(field, args))
    return field
}

foobar = _object_from({ message = 100 })

print "hello".length 1, 2, 3, 4, 5
print "hello world!".slice(1, -1)
print foobar.message
print foobar.foobar

