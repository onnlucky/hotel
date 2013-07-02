
ArgRef = n, s -> [#arg, s]
LocalRef = n, s -> [#local, s]
ScopeRef = n, d, s -> [#env, d, s]
ScopeArgRef = n, d, s -> [#envarg, d, s]
GlobalRef = n, s -> [#global, s]

Module = -> {
    globals: Array.new
    globalsIndex: HashMap.new
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

writeRef = buf, ref ->
    assert ref >= 0
    if ref < 1 << 7:
        buf.writeByte ref
        return
    ref = ref - 1 << 7
    if ref < 1 << 13:
        buf.writeByte 0b1000_0000 + ref >> 7
        buf.writeByte ref & 0b0111_1111
        return
    ref = ref - 1 << 13
    if ref < 1 << 19:
        buf.writeByte 0b1000_0000 + ref >> 13
        buf.writeByte 0b1000_0000 + (ref >> 7) & 0b0111_1111
        buf.writeByte ref & 0b0111_1111
        return
    assert false

literals = HashMap.new
var $size = 0

writeCode = v, main, sub, top ->
    print "code"
    code = Buffer.new
    write v.args, main, code
    write v.argnames, main, code
    write v.locals, main, code
    write v.name, main, code

    var $pc = 0
    loop:
        op = v.code[($pc += 1)]
        print "OP:", op
        | op == #end    | code.writeByte 0; break
        | op == #invoke | code.writeByte 192 + 13
        | op == #bind   |
            code.writeByte 192 + 11
            write v.code[($pc += 1)], main, code
        | op == #module |
            code.writeByte 192 + 6
            write v.code[($pc += 1)], main, code
        | op == #global | code.writeByte 192 + 6; writeRef code, v.code[($pc += 1)]
        | op == #mcall  | code.writeByte 224 + 0; writeRef code, v.code[($pc += 1)]
        | op == #fcall  | code.writeByte 224 + 1; writeRef code, v.code[($pc += 1)]
        | op == #bcall  | code.writeByte 224 + 2; writeRef code, v.code[($pc += 1)]
        |               | print "WARNING: skipping", op

    | code.size < 32 |
        main.writeByte 160 + code.size
    |             |
        main.writeByte 224 + 5
        writeSize main, v.size

    main.write code
    ref = $size; $size += 1
    if top: return
    writeRef sub, ref

write = v, main, sub, top ->
    | isCode(v)   | writeCode v, main, sub, top
    | v == null   | sub.writeByte 192 + 0
    | v == false  | sub.writeByte 192 + 1
    | v == true   | sub.writeByte 192 + 2
    | isFloat(v)  |
        return sub.writeByte 128 + 8
        if literals[v]: return writeRef sub, literals[v]
        ref = literals[v] = $size; $size += 1

        main.writeByte 9
        main.writeFloat v
        return writeRef sub, ref
    | isNumber(v) |
        if v >= -2 and v + 8 < 50: return sub.writeByte 192 + 8 + v
        if literals[v]: return writeRef sub, literals[v]
        ref = literals[v] = $size; $size += 1

        main.writeByte 192 + 4
        3.to 0: n -> main.writeByte v >> n * 8
        return writeRef sub, ref
    | isString(v) |
        if v.size == 0: return sub.writeByte 192 + 3
        if literals[v]: return writeRef sub, literals[v]
        ref = literals[v] = $size; $size += 1

        | v.size < 32 |
            main.writeByte 0 + v.size
            print "short string", v.size, v
        |              |
            main.writeByte 224 + 0
            writeSize main, v.size
        main.write v
        return writeRef sub, ref
    | isList(v)   |
        if v.size == 0: return sub.writeByte 192 + 4

        list = Buffer.new
        | v.size < 32 |
            list.writeByte 32 + v.size
        |             |
            list.writeByte 224 + 1
            writeSize main, v.size
        v.each: v => write v, main, list
        assert list.size >= 1 + v.size
        print "list", list.size
        main.write list
        ref = $size; $size += 1
        if top: return
        writeRef sub, ref
    | isObject(v) |
        keys = _Map_keys(v)
        if keys.size == 0: return sub.writeByte 192 + 5

        map = Buffer.new
        | keys.size < 32 |
            map.writeByte 96 + keys.size
        |                |
            list.writeByte 224 + 3
            writeSize main, v.size
        keys.each: k ->
            write k, main, map
            write _Map_get(v, k), main, map
        assert map.size >= 1 + keys.size * 2
        main.write map
        ref = $size; $size += 1
        if top: return
        writeRef sub, ref

test = {hello:"world",test:[true, false, null, "", [], {}, -1, 0, 1, 2, 50, 100]}
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

