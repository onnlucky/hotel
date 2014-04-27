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
    i2 = i - 1 << 7
    if i2 < 1 << 13: return bin(0b1000_0000 | i >> 7, i & 0b0111_1111)
    i3 = i - 1 << 13
    if i3 < 1 << 19: return bin(0b1000_0000 | i >> 13, 0b1000_0000 | (i >> 7) & 0b0111_1111, i & 0b0111_1111)
    assert false

encodeString = s ->
    assert isString s
    if s.size < 32: return bin(0b0000_0000 | s.size, s)
    return bin(0b1110_0000, encodeInt(s.size), s)

encodeList = l ->
    if l.size < 32:
        buf = Buffer.new 0b0010_0000 | l.size
        l.each: e -> buf.write e
        return buf.read
    buf = Buffer.new 0b1110_0001, encodeInt(l.size)
    l.each: e -> buf.write e
    return buf.read

encodeMap = m ->
    size = m.size >> 1
    assert size * 2 == m.size
    if size < 32:
        buf = Buffer.new 0b0110_0000 | size
        m.each: e -> buf.write e
        return buf.read
    buf = Buffer.new 0b1110_0011, encodeInt(size)
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
        { op == "envarg" }: buf.write 0b1100_0000 + 9; buf.write encodeInt c.code[($pc += 1)]; buf.write encodeInt c.code[($pc += 1)]
        { op == "local"  }: buf.write 0b1100_0000 + 10; buf.write encodeInt c.code[($pc += 1)]
        { op == "arg"    }: buf.write 0b1100_0000 + 11; buf.write encodeInt c.code[($pc += 1)]
        { op == "store"  }: buf.write 0b1100_0000 + 13; buf.write encodeInt c.code[($pc += 1)]
        { op == "rstore" }: buf.write 0b1100_0000 + 14; buf.write encodeInt c.code[($pc += 1)]; buf.write encodeInt c.code[($pc += 1)]
        { op == "invoke" }: buf.write 0b1100_0000 + 15
        { op == "mcall"  }: buf.write 0b1110_0000 + 0; buf.write encodeInt c.code[($pc += 1)]
        { op == "fcall"  }: buf.write 0b1110_0000 + 1; buf.write encodeInt c.code[($pc += 1)]
        { op == "bcall"  }: buf.write 0b1110_0000 + 2; buf.write encodeInt c.code[($pc += 1)]
        { op == "mcalln" }: buf.write 0b1111_0000 + 0; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)
        { op == "fcalln" }: buf.write 0b1111_0000 + 1; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)
        { op == "bcalln" }: buf.write 0b1111_0000 + 2; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)
        {                }: throw "unknown op: $op $(repr(op))"

    if buf.size < 32: return bin(0b1010_0000 | buf.size, buf.read)
    return bin(0b1110_0101, encodeInt(buf.size), buf)

opname = op ->
    { op == "==" }: return "eq"
    { op == "!=" }: return "neq"

    { op == ">" }: return "qt"
    { op == ">=" }: return "qte"
    { op == "<" }: return "lt"
    { op == "<=" }: return "lte"

    { op == "|" }: return "band"
    { op == "&" }: return "bor"
    { op == "<<" }: return "lshift"
    { op == ">>" }: return "rshift"

    { op == "+" }: return "add"
    { op == "-" }: return "sub"
    { op == "*" }: return "mul"
    { op == "/" }: return "div"
    { op == "/."}: return "idiv"
    { op == "%" }: return "mod"
    { op == "^" }: return "pow"

compile = (code ->
    globals = HashMap.new
    functions = Array.new

    Scope = parent -> {{parent},
        names = HashMap.new
        locals = Array.new
        addOpen = name, function, inMethods ->
            _, fn = functions.add {{name, inMethods}, open = function, scope = this}
            _, local = this.locals.add(name)
            local = local - 1
            this.names[name] = {{fn, local}, arg =false}
            return local, fn
        add = name ->
            _, local = this.locals.add(name)
            local = local - 1
            this.names[name] = {{local}, arg = false}
            return local
        addarg = name ->
            this.names[name] = {arg = true, local = this.names.size}
        ref = name, level ->
            ref = this.names[name]
            if try(ref.fn):
                if functions[ref.fn].open: close(ref.fn)
            if ref:
                if ref.arg:
                    if not level: return ["arg", ref.local]
                    return ["envarg", level, ref.local]
                if not level: return ["local", ref.local]
                return ["env", level, ref.local]
            if parent:
                return parent.ref name, 0
            globals[name] = true
            return ["global", name]
    }

    close = n ->
        ref = functions[n]
        functions[n] = {name=ref.name,open=false}
        fn = emittop ref.scope, ref.open, ref.inMethods
        functions[n] = {name=ref.name,open=false,function=fn}

    argspec = arg ->
        if arg.type == "lazy": return [arg.name, arg.d, true]
        if arg.d: return [arg.name, arg.d]
        [arg.name]

    emittop = scope, term, inMethods ->
        print "TOP TERM", repr(term)
        { term.type == "function" }:
            scope = Scope(scope)
            as = try(term.as) or []
            args = as.map(arg, n -> scope.addarg(arg.name); argspec(arg))
            code = term.body.map(stm -> emit scope, stm).flatten
            return {{code, args}, locals = scope.locals.toList}
        { term.type == "class" }:
            scope = Scope(scope)
            as = try(term.as) or []
            scope.addarg("this")
            args = as.map(arg, n -> scope.addarg(arg.name); argspec(arg))
            code = term.body.map(stm -> emit scope, stm).flatten
            return {{code, args}, locals = scope.locals.toList}
        { term.type == "block" }:
            scope = Scope(scope)
            as = try(term.as) or []
            args = as.map(arg, n -> scope.addarg(arg.name); argspec(arg))
            code = term.body.map(stm -> emit scope, stm).flatten
            return {{code, args}, locals = scope.locals.toList}

    whenPrimitive = v ->
        { isNumber(v) }: v
        { isString(v) }: v
        { isBool(v)   }: v
        {             }: null


    emit = scope, term, inMethods ->
        print "SUB TERM", repr(term)
        { term == "undefined"  }: ["undef"]
        { term == "null"  }: ["null"]
        { term == "false" }: ["false"]
        { term == "true"  }: ["true"]
        { isNumber(term)  }: ["int", term]
        { term.type == "string" }: ["data", term.data]
        { term.type == "function" or term.type == "class" or term.type == "block" }:
            function = emittop scope, term, inMethods
            _, fn = functions.add {name = "-", open = false, function = function}
            return ["bind", fn]
        { term.type == "assign" }:
            type = try(term.value.type)
            if type == "function" or type == "class" or type == "block":
                local, fn = scope.addOpen(term.to, term.value)
                return ["bind", fn, "store", local]
            local = scope.add(term.to)
            if term.to == "methods" and type == "object":
                v = emit scope, term.value, true
                return v + ["store", local]
            v = emit scope, term.value
            return v + ["store", local]
        { term.type == "call" }:
            args = try(term.args) or []
            fn = emit scope, term.target
            names = args.map: stm -> try(stm.n)
            args = args.map: stm -> emit scope, stm.v
            if try(term.block):
                function = emittop scope, term.block
                _, blk = functions.add {name="block",open=false,function=function}
                names = names.add "block"
                args = args.add ["bind", blk]
                print repr names
                print repr args
                return ["fcalln", args.size, {ns = names}] + fn + args + ["invoke"]
            if names.filter.size > 0: return ["fcalln", args.size, {ns = names}] + fn + args + ["invoke"]
            ["fcall", args.size] + fn + args + ["invoke"]
        { term.type == "method" }:
            args = try(term.args) or []
            target = emit scope, term.target
            names = args.map: stm -> try(stm.n)
            args = args.map: stm -> emit scope, stm.v
            if names.filter.size > 0: return ["mcalln", args.size, {ns = names}] + target + ["data", term.method] + args + ["invoke"]
            ["mcall", args.size] + target + ["data", term.method] + args + ["invoke"]
        { term.type == "op" }:
            rhs = emit scope, term.rhs
            lhs = emit scope, term.lhs
            ["bcall", 2, "global", opname(term.op), rhs, lhs, "invoke"]
        { term.type == "ref" }:
            scope.ref(term.name)
        { term.type == "object" }:
            m = HashMap.new
            elements = term.data.map(entry ->
                if try(entry.v.type) == "function":
                    function = emittop scope, entry.v, inMethods
                    _, fn = functions.add {name = entry.n, open = false, function = function}
                    return ["data", entry.n, "bind", fn]
                //scope.add(entry.n)
                m[entry.n] = whenPrimitive(entry.v)
                ["data", entry.n, emit(scope, entry.v, inMethods)]
            )
            //return ["data", m.toMap]
            ["fcall", elements.size * 2, "global", "__map"] + elements + ["invoke"]
        { term.type == "list" }:
            elements = term.data.map(entry -> [emit(scope, entry)])
            ["fcall", elements.size, "global", "__list"] + elements + ["invoke"]
        { }:
            print "ERROR"
            print repr(term)
            throw "unknown term $(term.type)"

    parseemit = text ->
        res = emittop Scope(null), parse(text)
        functions.each: fn, n -> if fn.open: close(n)
        functions.each: fn, n -> print "fn $n:", repr(fn)
        print "main:", repr(res)
        return res, functions

    builder_new = functions -> {
        functions = functions
        datas = Array.new
        dataIndex = HashMap.new
        globals = Array.new
        globalsIndex = HashMap.new
        functions = HashMap.new
        data = v ->
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
        global = v ->
            assert isString(v)
            ref = this.globalsIndex[v]
            if ref: return ref
            ref = this.data v
            _, at = this.globals.add ref
            this.globalsIndex[v] = encodeInt(at - 1)
        function = name, code, locals, argspecs, debuginfo ->
            code = code.add("end")
            bin = writeCode this, {{name, locals, argspecs, code, debuginfo}}
            _, at = this.datas.add bin
            print "function", at, name
            this.functions[name] = encodeInt(at - 1)
        code = v ->
            fn = functions[v]
            print "code:", repr(fn)
            return this.function(fn.name, fn.function.code, fn.function.locals, fn.function.args, {text="",offset=0,pos=[]})
        create = ->
            buf = Buffer.new
            buf.write "tl01"
            buf.write encodeInt this.datas.size + 1
            this.datas[:-2].each: v -> buf.write v
            buf.write encodeList this.globals
            buf.write this.datas[-1]
            return buf
    }

    compilemain = main, function ->
        name = try(main.name) or ""
        locals = try(main.locals) or []
        argspec = try(main.args) or []
        text = try(main.text) or ""
        pos = try(main.pos) or []

        builder = builder_new(functions)
        builder.function(name, main.code, locals, argspec, {text=text,offset=0,pos=pos})
        builder.create

    main, functions = parseemit(code)
    buf = compilemain(main, functions)
    return buf.read
)

print_pos = debugger ->
    pos, text = debugger.pos
    if not pos: return
    begin = (text.find('\n', pos, backward=true) or -1) + 1
    end = (text.find('\n', pos) or text.size + 1) - 1
    print text
    print " ".times(pos - 1) + "^"

SimpleDebugger = debugger ->
    loop:
        //catch: e -> print "oeps", e.toString
        print "INPUT>"
        at = io.stdin.find("\n")
        cmd = io.stdin.readString(at).strip
        { cmd == "call" }:
            print repr debugger.call
        { cmd == "locals" }:
            print repr debugger.locals
        { cmd == "list" or cmd == "pos" }:
            print_pos(debugger)
        { cmd == "step" or cmd == "" }:
            debugger.step
            print_pos(debugger)
        { cmd == "stop" }:
            debugger.detach debugger.current
            debugger.continue
            break

load = bytes ->
    m = _Module_new(Buffer.new(bytes), "")
    {
        bytes = bytes
        run = as -> _module_run(m, as)
        debug = as ->
            debugger = Debugger.new
            task = Task.new
            debugger.attach(task)
            _module_run(m, as, task)
            return SimpleDebugger(debugger)
    }

{{ compile, load }}
