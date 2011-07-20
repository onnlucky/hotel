Text = {
     size: ->{ _text_size(this) }
    empty: ->{ _text_size(this) == 0 }
    slice: (s, e)->{ _text_slice(this, s, e) }
}

%send = (oop, msg)->{
    field = %map_get(Text, msg)
    goto(_method_invoke(field, oop, args))
}

out("hello".size, "\n")
"hello".slice(1, -1)
