TextClass = [
    size = (this){ _text_size(this) }
    toText = (this){ this }
]
_send = (target, name){
    method = _map_get(TextClass, name)
    method(target)
}
//"foobar".size
_send("foobar", #size)

