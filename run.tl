Text = [
    length = (this){ _text_size(this) }
]
send = (oop, name){
    _map_get(Text, name)(oop)
    //send_apply(msg, map_get(Text, name))
}
"hello".length

