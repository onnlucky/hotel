fatal = { FATAL }
if = (cond, &tb, &fb){ goto(bool(cond, tb, fb)()) }
while = (&cond, body){
    again = continuation
    if cond(), (body(); again())
}
or  = (left, &right){ bool(left, {left}, right)() }
and = (left, &right){ bool(left, right, {left})() }
xor = (left,  right){ bool(left, bool(right, false, true), bool(right, true, false)) }

Text = [
    length = (this){ _text_size(this) }
]

_host_classes = [
    0, 1, 2, 3, 4, 5, Text, 7
]

_object_send2 = (object, class, name, args){
    field = _map_get(class, name)
    if field != undefined, (
        if _callable_is(field), goto(_object_invoke(field, object, args))
        return field
    )
    class = _map_get(class, #class)
    if class, goto(_message_send2(object, class, name, args))
    return null
}

print _text
print _value_type("HELLO")
print _value_type(1234)

_object_send = (object, name, args){
    if _object_is(object), goto(_object_send2(object, object, name, args))

    class = _map_get(_host_classes, _value_type(object))
    field = _map_get(class, name)
    field(object)
    //if _callable_is(field), goto(_object_invoke(field, object, args))
    //field
}

"hello".length

