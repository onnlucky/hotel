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
_message_send2 = (oop, class, name, args){
    field = _map_get(class, name)
    if isDefined(field), (
        if isCallable(field), goto(_invoke(field, oop, args))
        return field
    )
    class = _map_get(class, #type)
    if class, goto(_message_send2(oop, class, name, args))
    return null
}

_message_send = (oop, name, args){
    if isObject(oop), goto(_message_send2(oop, oop, name, args))

    class = _list_get(primitives, asPrimitive(oop))
    field = _map_get(class, name)
    if isCallable(field), goto(_invoke(field, oop, args))
    return field
}

"hello".length

