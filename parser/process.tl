
ArgRef = n, s -> [#arg, s]
LocalRef = n, s -> [#local, s]
ScopeRef = n, d, s -> [#env, d, s]
ScopeArgRef = n, d, s -> [#envarg, d, s]
GlobalRef = n, s -> [#global, s]

Module = -> {
    globals: Array.new
    globalsIndex: HashMap.new
    dataIndex: HashMap.new
    module: -> return this

    scopeLookup: name ->
        ref = this.globalsIndex.get name
        if ref: return GlobalRef(name, ref)

        n = this.globals.size
        this.globals.add(name)
        this.globalsIndex.set(name, n)
        assert this.globals.size == this.globalsIndex.size
        return GlobalRef(name, n)
    getData: d ->
        return [#module, d]
    addFunction: fn ->
        return [#bind, fn]
}

Scope = parent, module -> {
    parent: parent
    module: module

    args: HashMap.new
    localnames: Array.new
    locals: HashMap.new
    lookup: name ->
        // locals might shadow args
        ref = this.locals.get name
        if ref: return LocalRef(name, ref)
        arg = this.args.get name
        if arg: return ArgRef(name, arg)
        this.parent.scopeLookup name, 0
    scopeLookup: name, depth ->
        ref = this.locals.get name
        if ref: return ScopeRef(name, depth, ref)
        arg = this.args.get name
        if arg: return ScopeArgRef(name, depth, arg)
        this.parent.scopeLookup name, depth + 1
    arg: name ->
        this.args.set(name, this.args.size)
    store: name ->
        n = this.locals.size
        this.locals.set(name, n)
        this.localnames.add(name)
        assert this.localnames.get(this.locals.get(name) + 1) == name
        return n
}

isRef = what -> return what.type == #ref
isCall = what -> return what.type == #call
isSend = what -> return what.type == #send
isBind = what -> return what.type == #assign

isFunc = what -> return what.type == #fn

emit = scope, what ->
    //print "EMIT:", inspect(what)
    if isNumber what:
        if what >= -16 and what < 111: return [#int, what]
        return scope.module.getData(what)
    if isString what: return scope.module.getData(what)

    if isRef what: return scope.lookup what.name
    if isCall what: return emitCall scope, what
    if isSend what: return emitSend scope, what
    if isBind what: return emitBind scope, what

    if isFunc what: return emitFunc scope, what
    _throw "missing ... $(inspect what)"

emitBind = scope, store ->
    b = scope.store store.name
    return [#store, b]

emitCall = scope, call ->
    target = emit scope, call.target
    args = call.args.map: e -> emit scope, e
    return [#fcall, args.size, target].cat(args).cat([#invoke])

emitSend = scope, send ->
    target = emit scope, send.target
    args = send.args.map: e -> emit scope, e
    msg = scope.module.getData(send.msg)
    return [#mcall, args.size, target, msg].cat(args).cat([#invoke])

isCode = o -> return isObject(o) and o.code and o.args and o.argnames
emitFunc = parent, func ->
    scope = Scope(parent, parent.module)

    args = Array.new
    argnames = HashMap.new

    args = func.args.map: arg -> scope.arg arg.name; [arg.name, arg.type == #lazy, arg.default or null]
    args.each: a, n -> if a[1]: argnames[a[1]] = n - 1
    body = func.body.map(expr -> emit scope, expr).add(#end)
    fn = {
        name: "<anon>"
        args: args
        argnames: argnames.toMap
        locals: scope.localnames.toList
        code: body.flatten
    }
    if parent == parent.module: return fn
    return scope.module.addFunction(fn)

emitParse = name, parse ->
    print inspect parse
    print
    module = Module()
    code = emitFunc(module, {body:parse, args:[], name:name})

    mod = {
        link: module.globals.toList
        body: code
    }
    print inspect mod
    print
    return mod

name = args[1]
module = emitParse name, parser.parse(io.File(name).read)

// ** write **
//         8 |  7 |  6 |  5 |  4 |  3 |  2 |  1
// bytes: 128| 64 | 32 | 16 |  8 |  4 |  2 |  1
// TODO add -1.0 0.0 1.0 NaN -Inf +Inf ?
// format: tl01 ref [literals]*ref -> only a list of literals if ref is not a direct literal itself, last literal is root
// literal: number | string | list | map | set | binary | bytecode

// string: 1... .... byte*size      -> 7 bits size, utf8 encoded (0-127)
// list:   0100 .... [ref]*size     -> 4 bits size (0-15)
// map:    0101 .... [ref,ref]*size -> 4 bits size (0-15)
// binary: 0110 .... byte*size      -> 4 bits size, raw bytes (0-15)
// bcode:  0111 .... byte*size      -> 4 bits size, byte code (0-15)
// number: 0010 00.. sizebyte*n byte*size -> bignum encoded number, any length
// string: 0010 01.. sizebyte*n byte*size -> utf8 encoded
// binary: 0010 10.. sizebyte*n byte*size -> raw bytes
// bcode:  0010 11.. sizebyte*n byte*size -> byte code
//         0011 ....
// list:   0001 00.. sizebyte*n [ref]*size
// set:    0001 01.. sizebyte*n [ref]*size
// map:    0001 10.. sizebyte*n [ref,ref]*size
//         0001 11..
// number: 0000 0... byte*size      -> 1-8 byte signed int
// number: 0000 1000 byte*4         -> 4 byte double
// number: 0000 1001 byte*8         -> 8 byte double
//         0000 101.
//         0000 11..

//ref:
// 0... ....         -> reference: 7 bits        0     1      2     3   4   5   6   7   8 .. 63
// 10.. ....         -> literal: 6 bits, mapped: null, false, true, "", [], {}, -2, -1, 0, 1, ..
// 110. ....  byte*1 -> reference: 13 bits
// 1110 ....  byte*2 -> reference: 20 bits
// 1111 0...  byte*3 -> reference: 27 bits
// 1111 10..  byte*4 -> reference: 34 bits

sizebytes = size ->
    | size < 256      | 0
    | size < 65536    | 1
    | size < 16777216 | 2

writeRef = buf, ref ->
    assert ref >= 0
    if ref < 128: buf.writeByte ref; return
    ref = ref - 128
    if ref < 8192: buf.writeByte 192 + ref >> 8; buf.writeByte ref; return
    ref = ref - 8192
    if ref < 1048576: buf.writeByte 224 + ref >> 16; buf.writeByte ref >> 8; buf.writeByte ref; return
    assert false

literals = HashMap.new
var $size = 0

writeCode = v, main, sub, top ->
    code = Buffer.new
    write v.args, main, code
    write v.argnames, main, code
    write v.locals, main, code
    write v.name, main, code

    var $pc = 0
    loop:
        op = v.code[($pc += 1)]
        | op == #end  |
            code.writeByte 0; break
        | op == #bind |
            code.writeByte 192 + 13
            write v.code[($pc += 1)], main, code
        | op == #module |
            code.writeByte 192 + 6
            write v.code[($pc += 1)], main, code
        |               | print "skipping", op

    | code.size < 16 |
        main.writeByte 80 + code.size
    |             |
        bytes = sizebytes code.size
        main.writeByte 44 + bytes
        bytes.to 0: n -> main.writeByte code.size >> n * 8

    main.write code
    ref = $size; $size += 1
    if top: return
    writeRef sub, ref

write = v, main, sub, top ->
    | isCode(v)   | writeCode v, main, sub, top
    | v == null   | sub.writeByte 128 + 0
    | v == false  | sub.writeByte 128 + 1
    | v == true   | sub.writeByte 128 + 2
    | isFloat(v)  |
        return sub.writeByte 128 + 8
        if literals[v]: return writeRef sub, literals[v]
        ref = literals[v] = $size; $size += 1

        main.writeByte 9
        main.writeFloat v
        return writeRef sub, ref
    | isNumber(v) |
        if v >= -2 and v + 8 < 64: return sub.writeByte 128 + 8 + v
        if literals[v]: return writeRef sub, literals[v]
        ref = literals[v] = $size; $size += 1

        main.writeByte 0 + 3
        3.to 0: n -> main.writeByte v >> n * 8
        return writeRef sub, ref
    | isString(v) |
        if v.size == 0: return sub.writeByte 128 + 3
        if literals[v]: return writeRef sub, literals[v]
        ref = literals[v] = $size; $size += 1

        | v.size < 128 |
            main.writeByte 128 + v.size
        |              |
            bytes = sizebytes v.size
            main.writeByte 36 + bytes
            bytes.to 0: n -> main.writeByte v.size >> n * 8
        main.write v
        return writeRef sub, ref
    | isList(v)   |
        if v.size == 0: return sub.writeByte 128 + 4

        list = Buffer.new
        | v.size < 16 |
            list.writeByte 64 + v.size
        |             |
            bytes = sizebytes v.size
            list.writeByte 16 + bytes
            bytes.to 0: n -> list.writeByte v.size >> n * 8
        v.each: v => write v, main, list
        assert list.size >= 1 + v.size
        main.write list
        ref = $size; $size += 1
        if top: return
        writeRef sub, ref
    | isObject(v) |
        keys = _Map_keys(v)
        if keys.size == 0: return sub.writeByte 128 + 5

        map = Buffer.new
        | keys.size < 16 |
            map.writeByte 80 + keys.size
        |                |
            bytes = sizebytes keys.size
            map.writeByte 24 + bytes
            bytes.to 0: n -> map.writeByte keys.size >> n * 8
        keys.each: k ->
            write k, main, map
            write _Map_get(v, k), main, map
        assert map.size >= 1 + keys.size * 2
        main.write map
        ref = $size; $size += 1
        if top: return
        writeRef sub, ref

//test = {hello:"world",test:[true, false, null, "", -1, 0, 1, 2, 3, 0.0, 100]}
out = Buffer.new
out.write "tl01"
main = Buffer.new
write module, main, out, true
if main.size > 0:
    assert out.size == 4
    assert $size > 0
    writeRef out, $size
    out.write main

io.File(name+"b").write out

