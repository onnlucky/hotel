// hotel bytecode
//         8 |  7 |  6 |  5 |  4 |  3 |  2 |  1
// bytes: 128| 64 | 32 | 16 |  8 |  4 |  2 |  1
// TODO add -1.0 0.0 1.0 NaN -Inf +Inf ?
// format: tl01 ref [literals]*ref -> only a list of literals if ref is not a direct literal itself, last literal is root
// literal: number | string | list | map | set | binary | bytecode

// string: 000. .... byte*size       -> 5 bits size, utf8 encoded (1-32)
// list:   001. .... [ref]*size      -> 5 bits size (1-32)
// set:    010. .... [ref]*size      -> 5 bits size (1-32)
// map:    011. .... [ref,ref]*size  -> 5 bits size (1-32)
// raw:    100. .... byte*size       -> 5 bits size, raw bytes (1-32)
// bcode:  101. .... byte*size       -> 5 bits size, bytecode (1-32)

// number  1100 0... bytes*size      -> 1-8 byte signed int
// number  1100 1000 bytes*4         -> 4 byte float
// number  1100 1001 bytes*8         -> 8 byte float
// number  1100 1... bytes*size      -> (4,8,12,16,20,24,28,32) bytes bignum encoded number

// string  1110 0000 size byte*size  -> string, utf8 encoded
// list    1110 0001 size [ref]*size
// set     1110 0010 size [ref]*size
// map     1110 0011 size [ref,ref]*size
// raw     1110 0100 size byte*size  -> raw bytes
// bcode   1110 0101 size byte*size  -> byte code
// number  1110 0110 size byte*size  -> bignum encoded number

// references, direct literals, lengths   0     1      2     3   4   5   6   7   8
// 11.. .... -> 6 bits literals, indexed [null, false, true, "", [], {}, -2, -1, 0, 1, ...]
// 11.. .... -> in bytecode, an instruction, replaces literals
// 0... ....                               -> 7
// 10.. .... 0... ....                     -> 13
// 10.. .... 10.. .... 0... ....           -> 19
// 10.. .... 10.. .... 10.. .... 0... .... -> 25
// notice, there is madness in there: 111. .... is the mask for all CALL opcodes, and 1100 1011 is
// INVOKE, when opcode reference module data, they can still reference the empty list using
// 1100 0100 oeps ;) it will work, but maybe better have instructions to load those?

encodeLiteral = v ->
    { v == null  }: return 0b1100_0000 | 0
    { v == false }: return 0b1100_0000 | 1
    { v == true  }: return 0b1100_0000 | 2
    { v == ""    }: return 0b1100_0000 | 3
    { v == []    }: return 0b1100_0000 | 4
    { v == {}    }: return 0b1100_0000 | 5
    { v == -2    }: return 0b1100_0000 | 6
    { v == -1    }: return 0b1100_0000 | 7
    { v >= 0 and v <= 0b0011_1111 - 8 }: return 0b1100_0000 | (8 + v)
    {            }: return null

encodeInt = i ->
    assert i >= 0
    if i < 1 << 7: return i
    i = i - 1 << 7
    if i < 1 << 13: return bin(0b1000_0000 | i >> 7, i & 0b0111_1111)
    i = i - 1 << 13
    if i < 1 << 19: return bin(0b1000_0000 | i >> 13, 0b1000_0000 | (i >> 7) & 0b0111_1111, i & 0b0111_1111)
    assert false

encodeString = s ->
    assert isString s
    if s.size < 32: return bin(0b0000_0000 | s.size, s)
    return bin(0b1110_0000, encodeInt(s.size), s)

encodeList = l ->
    assert l.size < 32
    buf = Buffer.new 0b0010_0000 | l.size
    l.each: e -> buf.write e
    return buf.read

encodeMap = m ->
    size = m.size >> 1
    assert size * 2 == m.size
    assert size < 32
    buf = Buffer.new 0b0110_0000 | size
    m.each: e -> buf.write e
    return buf.read

writeCode = mod, c ->
    buf = Buffer.new

    buf.write mod.data(c.name)
    buf.write mod.data(c.argspecs)
    buf.write mod.data(c.locals)
    buf.write mod.data(c.debuginfo)

    var $pc = 0
    loop:
        op = c.code[($pc += 1)]
        { op == "end"    }: // end
            buf.write 0
            break
        { op == "system" }: // system call, references a string
            buf.write 0b1100_0000 + 5
            buf.write mod.data(c.code[($pc += 1)])
        { op == "data"   }: // a reference module data
            buf.write 0b1100_0000 + 6
            buf.write mod.data(c.code[($pc += 1)])
        { op == "global" }: // a reference to the linked list (a string)
            buf.write 0b1100_0000 + 7;
            buf.write mod.global(c.code[($pc += 1)])
        { op == "bind"   }: // bind a body into a closure, references bytecode stored in module data
            buf.write 0b1100_0000 + 12
            buf.write mod.code(c.code[($pc += 1)])
        { op == "true"   }: buf.write 0b1100_0000 + 0
        { op == "false"  }: buf.write 0b1100_0000 + 1
        { op == "null"   }: buf.write 0b1100_0000 + 2
        { op == "undef"  }: buf.write 0b1100_0000 + 3
        { op == "int"    }: buf.write 0b1100_0000 + 4; buf.write encodeInt c.code[($pc += 1)]
        { op == "env"    }: buf.write 0b1100_0000 + 8; buf.write encodeInt c.code[($pc += 1)]; buf.write encodeInt c.code[($pc += 1)]
        { op == "local"  }: buf.write 0b1100_0000 + 9; buf.write encodeInt c.code[($pc += 1)]
        { op == "arg"    }: buf.write 0b1100_0000 + 10; buf.write encodeInt c.code[($pc += 1)]
        { op == "result" }: buf.write 0b1100_0000 + 11; buf.write encodeInt c.code[($pc += 1)]
        { op == "store"  }: buf.write 0b1100_0000 + 13; buf.write encodeInt c.code[($pc += 1)]
        { op == "invoke" }: buf.write 0b1100_0000 + 14
        { op == "mcall"  }: buf.write 0b1110_0000 + 0; buf.write encodeInt c.code[($pc += 1)]
        { op == "fcall"  }: buf.write 0b1110_0000 + 1; buf.write encodeInt c.code[($pc += 1)]
        { op == "bcall"  }: buf.write 0b1110_0000 + 2; buf.write encodeInt c.code[($pc += 1)]
        {                }: throw "unknown op: $op"

    if buf.size < 32: return bin(0b1010_0000 | buf.size, buf.read)
    return bin(0b11100101, encodeInt(buf.size), buf)

builder_new = -> {
    datas: Array.new
    dataIndex: HashMap.new
    globals: Array.new
    globalsIndex: HashMap.new
    functions: HashMap.new
    data: v ->
        ref = encodeLiteral v
        if ref: return ref
        ref = this.dataIndex[v]
        if ref: return ref
        bin = { isList(v)   }: encodeList v.map(v -> this.data v)
              { isObject(v) }: encodeMap Object.map(v, (k, v -> [this.data(k), this.data(v)])).flatten
              { isString(v) }: encodeString v
        _, at = this.datas.add bin
        print "data", at, repr(v)
        this.dataIndex[v] = encodeInt(at - 1)
    global: v ->
        assert isString(v)
        ref = this.globalsIndex[v]
        if ref: return ref
        ref = this.data v
        _, at = this.globals.add ref
        this.globalsIndex[v] = encodeInt(at - 1)
    function: name, code, locals, argspecs, debuginfo ->
        code = code.add("end")
        bin = writeCode this, {{name, locals, argspecs, code, debuginfo}}
        _, at = this.datas.add bin
        print "function", at, name
        this.functions[name] = encodeInt(at - 1)
    code: v ->
        assert isList(v)
    create: ->
        buf = Buffer.new
        buf.write "tl01"
        buf.write encodeInt this.datas.size + 1
        this.datas[:-2].each: v -> buf.write v
        buf.write encodeList this.globals
        buf.write this.datas[-1]
        return buf
}

Module = {
    new: (main ->
        name = try(main.name) or ""
        locals = try(main.locals) or []
        argspec = try(main.args) or []
        text = try(main.text) or ""
        pos = try(main.pos) or []

        builder = builder_new()
        builder.function(name, main.code, locals, argspec, {text: text, offset: 0, pos: pos})
        buf = builder.create
        m = _Module_new(buf, "")
        {
            run: args -> _module_run(m, args or [])
        }
    )
}

Module.new({
    code: [
        "fcall", 1
        "global", "print"
        "data", "hello world"
        "invoke"
    ]
}).run

/*
Module.new({
    name: "main"
    text: "a, b, c -> print(a, b, c)"
    code: [
        "fcall", 3
        "global", "print"
        "arg", 0
        "arg", 1
        "arg", 2
        "invoke"
    ]
    pos: [
        12, 12
        12, 12
        18, 18
        21, 21
        24, 24
        12
    ]
}).run

/*

print "LOADING MODULE"
m = _Module_new(buf, "test")
debugger = Debugger.new
task = Task.new
debugger.attach(task)
catch: e -> print "Exception:", e
_module_run(m, [["3"],["2","b"],["4"],["1","a"]], task) // task


print_pos = ->
    pos, text = debugger.pos
    if not pos: return
    begin = (text.find('\n', pos, backward=true) or -1) + 1
    end = (text.find('\n', pos) or text.size + 1) - 1
    print text
    print " ".times(pos - 1) + "^"

print "DEBUGGING"
loop:
    catch: e -> print "oeps", e.toString
    at = io.stdin.find("\n")
    cmd = io.stdin.readString(at).strip
    { cmd == "call" }:
        print repr debugger.call
    { cmd == "locals" }:
        print repr debugger.locals
    { cmd == "list" or cmd == "pos" }:
        print_pos()
    { cmd == "step" or cmd == "" }:
        debugger.step
        print_pos()

