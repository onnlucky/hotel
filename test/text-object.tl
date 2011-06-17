Text = {
     size: { %text_size(this) }
    empty: { %text_size(this) == 0 }
    slice: (s, e){ %text_slice(this, s, e) }
}

%send = (oop, msg, *as){
    field = %map_get(Text, msg)
    goto(%invoke(oop, field, as))
}

out("hello".size, "\n")
"hello".slice(1, -1)
