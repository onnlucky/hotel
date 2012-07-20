MAGIC = "tl01"
SYMBOL = 1
STRING = 2
CODE = 3

num = { b, n ->
    if n < 128: return b.writeByte n
}
sym = { b, s ->
    b.writeByte SYMBOL
    t = s.toText
    num b, t.size
    b.write t
}
string = { b, s ->
    b.writeByte STRING
    num b, s.size
    b.write s
}
/*
code = { b, c ->
    t = Buffer.new
    pc = Var.new 0
    while $pc < c.size: {
        match c[pc++]
        || #end  -> t.writeByte 0
        || #call -> t.writeByte 1; num t, c[pc++]
        || #data -> t.writeByte 2; num t, c[pc++]
        || #eval -> t.writeByte 3
    }

    b.writeByte CODE
    num b, t.size
    b.write t
}
*/
emit = { c ->
    b = _Buffer_new()
    var $pc = 0
    while $pc < c.size: {
        op = c.get($pc += 1)
        _match(op == #end): b.writeByte 0
        _match(op == #call): b.writeByte 1; num b, c.get($pc += 1)
        _match(op == #data): b.writeByte 2; num b, c.get($pc += 1)
        _match(op == #eval): b.writeByte 3
    }
    return b
}
code = { b, c ->
    t = emit(c)
    b.writeByte CODE
    num b, t.size
    b.write t
}

b = _Buffer_new()

b.write MAGIC
num b, 3
sym b, #print
string b, "hello world!"
code b, [#call, 1, #data, 0, #data, 1, #eval, #end]

io.File("out.tlb").write b

