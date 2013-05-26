
ArgRef = n, s -> [#arg, s]
LocalRef = n, s -> [#local, s]
ScopeRef = n, d, s -> [#env, d, s]
ScopeArgRef = n, d, s -> [#envarg, d, s]
GlobalRef = n, s -> [#global, s]

Module = -> {
    globals: Array.new
    globalsIndex: HashMap.new
    data: Array.new
    dataIndex: HashMap.new

    scopeLookup: name ->
        ref = this.globalsIndex.get name
        if ref: return GlobalRef(name, ref)

        n = this.globals.size
        this.globals.add(name)
        this.globalsIndex.set(name, n)
        assert this.globals.size == this.globalsIndex.size
        return GlobalRef(name, n)
    getData: datum ->
        ref = this.dataIndex.get(datum)
        if ref: return [#module, ref]

        n = this.data.size
        this.data.add(datum)
        this.dataIndex.set(datum, n)
        assert this.data.size >= this.dataIndex.size
        return [#module, n]
    addFunction: fn ->
        n = this.data.size
        this.data.add(fn)
        return [#bind, n]
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

emitFunc = parent, func ->
    scope = Scope(parent, parent.module)

    args = Array.new
    lazies = Array.new
    defaults = Array.new
    defaults = HashMap.new

    args = func.args.map: arg -> scope.arg arg.name; [arg.name, arg.type == #lazy, arg.default or null]
    body = func.body.map(expr -> emit scope, expr).add(#end)
    fn = {
        name: "<anon>"
        args: args
        locals: scope.localnames.toList
        code: body.flatten
    }
    return scope.module.addFunction(fn)

emitParse = name, parse ->
    module = Module()
    scope = Scope(module, module)

    body = parse.map: expr -> emit scope, expr
    code = body.cat([#end]).flatten

    mod = {
        data: module.data.toList
        body: code
        globals: module.globals.toList
        name: name
    }
    print inspect mod
    return mod

name = args[1]
module = emitParse name, parser.parse(io.File(name).read)

// ** write **
//         8 |  7 |  6 |  5 |  4 |  3 |  2 |  1
// bytes: 128| 64 | 32 | 16 |  8 |  4 |  2 |  1

// format: tl01 ref [literals]*ref -> only a list of literals if ref is not a direct literal itself, last literal is root
// literal: number | string | list | map

// string: 1... .... byte*size      -> 7 bits size, utf8 encoded (0-127)
// list:   010. .... [ref]*size     -> 5 bits size (0-31)
// map:    011. .... [ref,ref]*size -> 5 bits size (0-31)
// number: 0000 0... sizebyte*n byte*size -> 1-8 byte signed int
// number: 0000 1... sizebyte*n byte*size -> 1-8 byte double
// number: 0001 0... sizebyte*n byte*size -> bignum encoded number
// string: 0001 1... sizebyte*n byte*size -> long string, utf8 encoded
// list:   0010 0... sizebyte*n [ref]*size     -> long string, utf8 encoded
// map:    0010 1... sizebyte*n [ref,ref]*size -> long string, utf8 encoded

//ref:
// 0... ....         -> reference: 7 bits                0     1      2     3   4   5   6   7   8
// 10.. ....         -> direct literal: 0-63, mapped as: null, false, true, "", [], {}, -2, -1, 0, 1, ..
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

write = v, main, sub, top ->
    assert main
    assert sub
    | v == null   | sub.writeByte 128 + 0
    | v == false  | sub.writeByte 128 + 1
    | v == true   | sub.writeByte 128 + 2
    | isNumber(v) |
        if v >= -2 and v + 8 < 64: return sub.writeByte 128 + 8 + v
        if literals[v]: return writeRef sub, literals[v]

        ref = literals[v] = $size; $size += 1
        main.writeByte 0 + 3
        1.upto 4: n -> main.writeByte v >> n * 8
        return writeRef sub, ref
    | isString(v) |
        if v.size == 0: return sub.writeByte 128 + 3
        if literals[v]: return writeRef sub, literals[v]

        ref = literals[v] = $size; $size += 1
        | v.size < 128 |
            main.writeByte 128 + v.size
            main.write v
        |              |
            bytes = sizebytes v.size
            main.writeByte 24 + bytes
            bytes.upto 0: n -> main.writeByte v.size >> n * 8
            main.write v
        return writeRef sub, ref
    | isList(v)   |
        if v.size == 0: return sub.writeByte 128 + 4

        list = Buffer.new
        | v.size < 32 |
            list.writeByte 64 + v.size
        |             |
            bytes = sizebytes v.size
            list.writeByte 24 + bytes
            bytes.upto 0: n -> list.writeByte v.size >> n * 8
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
        | keys.size < 32 |
            map.writeByte 96 + keys.size
        |                |
            bytes = sizebytes keys.size
            map.writeByte 24 + bytes
            bytes.upto 0: n -> map.writeByte keys.size >> n * 8
        keys.each: k ->
            write k, main, map
            write _Map_get(v, k), main, map
        assert map.size >= 1 + keys.size * 2
        main.write map
        ref = $size; $size += 1
        if top: return
        writeRef sub, ref

out = Buffer.new
out.write "tl01"
main = Buffer.new
write {hello:"world",test:[true, false, null, "", -1, 0, 1, 2, 3]}, main, out, true
if main.size > 0:
    assert out.size == 4
    assert $size > 0
    writeRef out, $size
    out.write main

io.File(name+"b").write out

