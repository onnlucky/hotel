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
// number  1100 1... bytes*size      -> (12,16,20,24,28,32,36,40) bytes bignum encoded number, not used?

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
    if i >>  0 < 1 << 7: return i
    if i >>  7 < 1 << 6: return Bin(0b1000_0000 | i >> 7, i & 0b0111_1111)
    if i >> 13 < 1 << 6: return Bin(0b1000_0000 | i >> 13, 0b1000_0000 | (i >> 7 & 0b0011_1111), i & 0b0111_1111)
    if i >> 19 < 1 << 6: return Bin(0b1000_0000 | i >> 19, 0b1000_0000 | (i >> 13 & 0b0011_1111),
                                    0b1000_0000 | (i >> 7 & 0b0011_1111), i & 0b0111_1111)
    if i >> 25 < 1 << 6: return Bin(0b1000_0000 | i >> 25, 0b1000_0000 | (i >> 19 & 0b0011_1111),
                                    0b1000_0000 | (i >> 13 & 0b0011_1111), 0b1000_0000 | (i >> 7 & 0b0011_1111), i & 0b0111_1111)
    assert false

encodeFloat = f ->
    Bin(0b1100_1001) + f.bytes

encodeNumber = num ->
    pos = num >= 0
    n = num.abs
    if n < 1 <<   7: return Bin(0b1100_0000 | 0,                           n << 1 | bool(pos,0,1))
    if n < 1 <<  15: return Bin(0b1100_0000 | 1,                   n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  23: return Bin(0b1100_0000 | 2,          n >> 15, n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  31: return Bin(0b1100_0000 | 3, n >> 23, n >> 15, n >> 7, n << 1 | bool(pos,0,1))

    if n < 1 <<  39: return Bin(0b1100_0000 | 4,                            n >> 31, n >> 23, n >> 15, n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  47: return Bin(0b1100_0000 | 5,                   n >> 39, n >> 31, n >> 23, n >> 15, n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  55: return Bin(0b1100_0000 | 6,          n >> 47, n >> 39, n >> 31, n >> 23, n >> 15, n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  63: return Bin(0b1100_0000 | 7, n >> 55, n >> 47, n >> 39, n >> 31, n >> 23, n >> 15, n >> 7, n << 1 | bool(pos,0,1))

    bytes = num.bytes
    return Bin(0b1110_0110, encodeInt(bytes.size), bytes)

encodeString = s ->
    assert isString s
    if s.bytes < 32: return Bin(0b0000_0000 | s.bytes, s)
    return Bin(0b1110_0000, encodeInt(s.bytes), s)

encodeList = l ->
    if l.size < 32:
        buf = Buffer.new(0b0010_0000 | l.size)
        l.each: e -> buf.write(e)
        return buf.read
    buf = Buffer.new(0b1110_0001, encodeInt(l.size))
    l.each: e -> buf.write(e)
    return buf.read

encodeMap = m ->
    size = m.size >> 1
    assert size * 2 == m.size
    if size < 32:
        buf = Buffer.new(0b0110_0000 | size)
        m.each: e -> buf.write(e)
        return buf.read
    buf = Buffer.new(0b1110_0011, encodeInt(size))
    m.each: e -> buf.write(e)
    return buf.read

writeCode = mod, c, scope ->
    buf = Buffer.new

    buf.write(mod.data(c.name))
    buf.write(mod.data(c.argspecs)) // TODO split up in 3 lists argnames, argdefaults, arglazy, null should be fine for any
    buf.write(mod.data(c.locals))
    { scope }:
        buf.write(mod.data(scope?localvars)) // mutable or not, null is fine if no mutables
    {       }:
        buf.write(mod.data(null))
    buf.write(mod.data(c.debuginfo))

    var $pc = 0
    loop:
        op = c.code[($pc += 1)]
        { op == "end"    }: // end
            buf.write(0)
            break
        { op == "system" }: // system call, references a string
            buf.write(0b1100_0000 + 5)
            buf.write(mod.data(c.code[($pc += 1)]))
        { op == "data"   }: // a reference module data
            buf.write(0b1100_0000 + 6)
            buf.write(mod.data(c.code[($pc += 1)], true))
        { op == "global" }: // a reference to the linked list (a string)
            buf.write(0b1100_0000 + 7)
            buf.write(mod.global(c.code[($pc += 1)]))
        { op == "bind"   }: // bind a body into a closure, references bytecode stored in module data
            buf.write(0b1100_0000 + 16)
            buf.write(mod.code(c.code[($pc += 1)]))
        { op == "true"   }: buf.write(0b1100_0000 + 0)
        { op == "false"  }: buf.write(0b1100_0000 + 1)
        { op == "null"   }: buf.write(0b1100_0000 + 2)
        { op == "undef"  }: buf.write(0b1100_0000 + 3)
        { op == "int"    }: buf.write(0b1100_0000 + 4); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "envarg" }: buf.write(0b1100_0000 + 8); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "env"    }: buf.write(0b1100_0000 + 9); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "arg"    }: buf.write(0b1100_0000 + 10); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "local"  }: buf.write(0b1100_0000 + 11); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "args"   }: buf.write(0b1100_0000 + 12)
        { op == "envargs"}: buf.write(0b1100_0000 + 13); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "this"   }: buf.write(0b1100_0000 + 14)
        { op == "envthis"}: buf.write(0b1100_0000 + 15); buf.write(encodeInt(c.code[($pc += 1)]))

        { op == "store"  }: buf.write(0b1100_0000 + 17); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "rstore" }: buf.write(0b1100_0000 + 18); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "invoke" }: buf.write(0b1100_0000 + 19)
        { op == "cerr"   }: buf.write(0b1100_0000 + 20)

        { op == "vget"    }: buf.write(0b1100_0000 + 21); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "vstore"  }: buf.write(0b1100_0000 + 22); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "vrstore" }: buf.write(0b1100_0000 + 23); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "evget"   }: buf.write(0b1100_0000 + 24); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "evstore" }: buf.write(0b1100_0000 + 25); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "evrstore"}: buf.write(0b1100_0000 + 26); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(encodeInt(c.code[($pc += 1)]))

        { op == "mcall"  }: buf.write(0b1110_0000 + 0); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "mcalls" }: buf.write(0b1110_1000 + 0); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "fcall"  }: buf.write(0b1110_0000 + 1); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "bcall"  }: buf.write(0b1110_0000 + 2); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "ccall"  }: buf.write(0b1110_0000 + 3); buf.write(encodeInt(c.code[($pc += 1)]))
        { op == "mcalln" }: buf.write(0b1111_0000 + 0); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(mod.data(c.code[($pc += 1)].ns))
        { op == "mcallns"}: buf.write(0b1111_1000 + 0); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(mod.data(c.code[($pc += 1)].ns))
        { op == "fcalln" }: buf.write(0b1111_0000 + 1); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(mod.data(c.code[($pc += 1)].ns))
        { op == "bcalln" }: buf.write(0b1111_0000 + 2); buf.write(encodeInt(c.code[($pc += 1)])); buf.write(mod.data(c.code[($pc += 1)].ns))

        {                }: throw "unknown op: $op $(repr(op))"

    if buf.size < 32: return Bin(0b1010_0000 | buf.size, buf.read)
    return Bin(0b1110_0101, encodeInt(buf.size), buf)

opname = op ->
    { op == "or"  }: op
    { op == "and" }: op
    { op == "xor" }: op
    { op == "not" }: op
    { op == "isa" }: op

    { op == "==" }: return "eq"
    { op == "!=" }: return "neq"

    { op == ">" }: return "gt"
    { op == ">=" }: return "gte"
    { op == "<" }: return "lt"
    { op == "<=" }: return "lte"

    { op == "~" }: return "bnot"
    { op == "&" }: return "band"
    { op == "|" }: return "bor"
    { op == "^" }: return "bxor"
    { op == "<<" }: return "lsh"
    { op == ">>" }: return "rsh"
    { op == ">>>" }: return "rrsh"

    { op == "+" }: return "add"
    { op == "-" }: return "sub"
    { op == "*" }: return "mul"
    { op == "/" }: return "div"
    { op == "/."}: return "idiv" // TODO fdiv
    { op == "//"}: return "idiv"
    { op == "%" }: return "mod"
    { op == "**" }: return "pow"

compile = (text, ast ->
    globals = HashMap.new
    functions = Array.new
    ids = Array.new

    Scope = { parent, block ->
        parentVisible = parent?locals.size or 0
        block = block or false
        names = HashMap.new
        locals = Array.new
        args = Array.new
        // TODO addOpen vs add, when function is named is clumsy, either collapse at assign, or do this
        addOpen = function, name, pos ->
            type = "local"
            _, fn = functions.add({name=name or "",open=function,scope=this})
            if name:
                _, id = ids.add({{name,type},at=Array.new([pos])})
                _, at = this.locals.add(name)
                this.names[name] = {arg=false,{at,id,fn,type}}
            return fn
        add = name, pos, type ->
            type = type or "local"
            if this.locals[-1] == name and this.names[name]?fn:
                ref = this.names[name]
                return ref.at, ref.id
            _, id = ids.add({{name,type},at=Array.new([pos])})
            _, at = this.locals.add(name)
            this.names[name] = {arg=false,{at,id,type}}
            return at, id
        close = ->
            this.names.each(name, ref ->
                if functions[ref?fn]?open: closefn(ref.fn)
            )
        localvars = ->
            this.locals.map(name -> this.names[name]?type == "var")
        addarg = name, pos ->
            _, id = ids.add({{name},at=Array.new([pos])})
            _, at = this.args.add(name)
            this.names[name] = {arg=true,{at,id}}
            return id
        greturn = level ->
            if this.block: return parent.greturn(level + 1)
            {type="return",scope=level}
        ggoto = level ->
            if this.block: return parent.ggoto(level + 1)
            {type="goto",scope=level}
        gbreak = level, ls ->
            if this.block: return parent.gbreak(level + 1, ls.add(level))
            if this.parent: return parent.gbreak(level + 1, ls)
            {type="break",scope=ls}
        gcontinue = level, ls ->
            if this.block: return parent.gcontinue(level + 1, ls.add(level))
            if this.parent: return parent.gcontinue(level + 1, ls)
            {type="continue",scope=ls}
        ref = name, level ->
            if name == "return": return this.greturn(0)
            if name == "goto": return this.ggoto(0)
            if name == "break": return this.gbreak(0, [])
            if name == "continue": return this.gcontinue(0, [])
            ref = this.names[name]
            if ref?fn and not args["peek"]:
                if functions[ref.fn].open: closefn(ref.fn)
            if ref:
                if ref.arg:
                    if not level: return {type="arg",at=ref.at,id=ref.id}
                    return {type="envarg",up=level,at=ref.at,id=ref.id}
                if not level: return {type="local",at=ref.at,id=ref.id,stype=ref.type}
                return {type="env",up=level,at=ref.at,id=ref.id,stype=ref.type}
            if name == "args" and not this.block:
                if not level: return {type="args"}
                return {type="envargs",up=level}
            if name == "this" and not this.block:
                if not level: return {type="this"}
                return {type="envthis",up=level}
            if parent:
                if not level: return parent.ref(name, 1)
                return parent.ref(name, level + 1)
            globals[name] = true
            return {type="global",{name}}
    }

    ClassScope = { parent ->
        parentVisible = parent?locals.size or 0
        block = false
        names = HashMap.new
        locals = Array.new
        args = Array.new
        methods = Array.new
        open = Var.new(true) // methods are "closed" after constructor/statics are initialized, methods references over "this", other over locals
        addOpen = function, name, pos ->
            type = "local"
            _, method = functions.add({name=name or "",open=function,scope=this})
            if name:
                // TODO perhaps change type="function" to type="method"
                _, id = ids.add({{name,type},at=Array.new([pos])})
                _, at = this.methods.add(name)
                this.names[name] = {arg=false,{at,id,method,type}}
            return method
        add = name, pos, type ->
            type = type or "local"
            if this.methods[-1] == name: // TODO not entirely correct ...
                ref = this.names[name]
                return ref.at, ref.id
            _, id = ids.add({{name,type},at=Array.new([pos])})
            _, at = this.locals.add(name)
            this.names[name] = {arg=false,{at,id,type}}
            return at, id
        addarg = name, pos ->
            _, id = ids.add({{name},at=Array.new([pos])})
            _, at = this.args.add(name)
            this.names[name] = {arg=true,{at,id}}
            return id
        close = ->
            this.open.set(false)
            this.names.each(name, ref ->
                if functions[ref?fn]?open: closefn(ref.fn)
                //if functions[ref?method]?open: closefn(ref.method)
            )
        ref = name, level ->
            ref = this.names[name]
            if not this.open.get:
                assert level
                assert name != "this"
                assert name != "args"
                if ref: return {type="field",name=name,up=level,at=ref.at,id=ref.id,stype=ref?type}
                return parent.ref(name, level)
            if ref:
                if ref.arg:
                    if not level: return {type="arg",at=ref.at,id=ref.id}
                    return {type="envarg",up=level,at=ref.at,id=ref.id}
                if not level: return {type="local",at=ref.at,id=ref.id,stype=ref?type}
                return {type="env",up=level,at=ref.at,id=ref.id,stype=ref?type}
            if name == "args" and not this.block:
                if not level: return {type="args"}
                return {type="envargs",up=level}
            if name == "this":
                assert false // TODO what to do here? should reference our (incomplete!) local here
                if not level: return {type="this"}
                return {type="envthis",up=level}
            if not level: return parent.ref(name, 1)
            parent.ref(name, level + 1)
    }

    closefn = n ->
        ref = functions[n]
        functions[n] = {name=ref.name,open=false}
        fn = lextop(ref.scope, ref.open)
        functions[n] = {name=ref.name,open=false,function=fn}

    argspec = arg ->
        if arg.type == "lazy": return [arg.name, arg.d, true]
        if arg.d: return [arg.name, arg.d]
        [arg.name]

    lextop = scope, term, linkframe ->
        scope = Scope(scope, term.type != "function")
        if isFrame(linkframe):
            var $at = 0
            loop:
                _, name, type, pos = linkframe.get(($at += 1))
                if not name: break
                scope.add(name, pos, type=type)
        { term.type == "function" }:
            args = (term?as or []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            scope.close
            {type="function",{scope,args,body},locals=scope.locals.toList,pos=term.pos}
        { term.type == "block" }:
            args = (term?as or []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            scope.close
            {type="block",{scope,args,body},locals=scope.locals.toList,pos=term.pos}
        { term.type == "scope" }:
            args = (term?as or []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            scope.close
            {type="scope",{scope,args,body},locals=scope.locals.toList,pos=term.pos}

    lex = scope, term, name, pos ->
        { term == null              }: "null"
        { isChar(term)              }: {type="call",target={type="global",name="Char"},args=[term.toNumber]}
        { not term?type             }: term
        { term.type == "string"     }: term
        { term.type == "stringlist" }:
            data = term.data.map(s ->
                { isString(s) }: {type="string",data=s}
                {             }: lex(scope, s)
            )
            {type="call",target={type="global",name="_String_cat"},args=data}
        { term.type == "scope" }:
            function = lextop(scope, term)
            _, fn = functions.add({name="",open=false,function=function})
            {type="call",target={type="function",function=fn}}
        { term.type == "function" or term.type == "block" }:
            function = scope.addOpen(term, name, pos)
            if not name: closefn(function)
            {type="function",{function}}
        { term.type == "class" }:
            outerscope = scope
            scope = ClassScope(scope)
            args = (term?as or []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            var $extend = null
            body = term.body.map(stm ->
                if stm?type == "call" and stm.target.name == "extend":
                    $extend = {type="method",target=stm.args[1].v,method="methods"}
                    return null
                lex(scope, stm)
            ).filter
            scope.close
            m = HashMap.new
            methods = body.filter(stm -> not (stm?type == "assign" and stm.value?type == "function")).map(stm -> m[stm.to.name] = null; [stm.to.name,stm.value])
            m["class"] = null
            constructor = body.filter(stm -> stm?type == "assign" and stm.value?type == "function")
            fields = scope.locals.map(name -> name) + scope.args.map(name -> name)
            class = [name=name, call=null, methods=null]
            /*
            print "---- class: $class ----"
            print "constructor", repr(constructor)
            print "fields", repr(fields)
            print "methods", repr(methods)
            print "class", repr(class), "extends", $extend
            print "---- class: $class ----"
            print repr(functions[1])
            */
            constructor = {type="function",{scope,args},locals=scope.locals.toList,pos=term.pos,body=constructor + [{type="system",name="createobject"}]}
            _, fn = functions.add({name="",open=false,function=constructor})
            c = { $extend }: {type="object", fill=[["call", {type="function",function=fn}],["class",lex(scope.parent,$extend)]], map=(class["class"]=null)}
                {         }: {type="object", fill=[["call", {type="function",function=fn}]], map=class}
            {type="class", class=c, methods={type="object", fill=methods, map=m.toMap}}
        { term.type == "assign" }:
            { term.to.type == "local" }:
                value = lex(scope, term.value, term.to.name, term.to.pos)
                ref = scope.ref(term.to.name, peek=true)
                type = ids[ref?id]
                if type?type == "var" or type?stype == "var":
                    return {type="vset",target=ref,{value}}
                local, id = scope.add(term.to.name, term.to.pos)
                to={type="local",name=term.to.name,pos=term.to.pos,at=local,{id}}
                {type="assign",{to,value}}
            { term.to.type == "newvar" }:
                local, id = scope.add(term.to.name, term.to.pos, type="var")
                value = lex(scope, term?value)
                {type="vset",target=scope.ref(term.to.name),{value}}
            { term.to.type == "var"    }:
                ref = scope.ref(term.to.name)
                assert ids[ref.id].type == "var"
                value = lex(scope, term.value)
                {type="vset",target=ref,{value}}
        { term.type == "multiassign" }:
            // TODO perhaps function defined this way must stay open?
            // TODO fixup for new var stuff
            value = lex(scope, term.value)
            locals = term.multi.map(to ->
                { to.type == "local" }: scope.add(to.name, to.pos)
                {                    }: scope.add("", to.pos) // anonymous locals for Var.new and var.set
                                        assert false
            )
            assigns = locals.map(l, n ->
                to = term.multi[n]
                { to.type == "local" }: null // nothing to do
                { to.type == "newvar" }:
                    at, id = scope.add(to.name, to.pos)
                    value = {type="method",target={type="global",name="Var"},method="new",args=[{type="local",at=l.at,id=l.id}]}
                    to={type="local",name=to.name,pos=to.pos,{at,id}}
                    {type="asign",{to,value}}
                { to.type == "var" }:
                    ref = scope.ref(to.name)
                    value = {type="local",at=l.at,id=l.id}
                    {type="method",target=ref,method="set",args=[value]}
            )
            {type="multiassign",{value,locals,assigns}}
        { term.type == "call" }:
            target = lex(scope, term.target)
            names = (term?args or []).map: stm -> stm?n
            args = (term?args or []).map: stm -> lex(scope, stm.v)
            block = { term?block }: lex(scope, term.block)
                    {            }: null
            if target?type == "return" or target?type == "goto":
                t = {type="global",name=target.type}
                return {type="call",target=t,args=[target.scope]+args,names=[null]+names,{block}}
            if target?type == "break" or target?type == "continue":
                t = {type="global",name=target.type}
                s = lex(scope, {type="list",data=target.scope})
                return {type="call",target=t,args=[s]+args,names=[null]+names,{block}}
            {type="call",{target,args,names,block}}
        { term.type == "method" or term.type == "safemethod" }:
            target = lex(scope, term.target)
            names = (term?args or []).map: stm -> stm?n
            args = (term?args or []).map: stm -> lex(scope, stm.v)
            block = { term?block }: lex(scope, term.block)
                    {            }: null
            {type=term.type,method=term.method,{target,args,names,block}}
        { term.type == "async" }:
            body = { term?body }: term.body
                   {           }: [term.rhs]
            function = lextop(scope, {type="scope",{body},pos=term.pos})
            _, fn = functions.add({name="",open=false,function=function})
            {type="async",body={type="function",function=fn}}
        { term.type == "not" }:
            rhs = lex(scope, term.rhs)
            {type="not",{rhs}}
        { term.type == "~" }:
            rhs = lex(scope, term.rhs)
            {type="~",{rhs}}
        { term.type == "-" }:
            rhs = lex(scope, term.rhs)
            {type="-",{rhs}}
        { term.type == "op"  }:
            lhs = lex(scope, term.lhs)
            rhs = lex(scope, term.rhs)
            {type="op",op=term.op,{lhs,rhs}}
        { term.type == "ref" }:
            return scope.ref(term.name)
        { term.type == "varref" }:
            return scope.ref(term.name)
        { term.type == "setfieldop" }:
            target = lex(scope, term.target)
            lhs = {type="method",method=term.field,{target}}
            rhs = lex(scope, term.rhs.v)
            op = {type="op",op=term.op,{lhs,rhs}}
            {type="method",method="_set",args=[{type="string",data=term.field},op],pos=term.pos,{target}}
        { term.type == "assignop" }:
            ref = scope.ref(term.to)
            if ref.type == "global" or ref?stype != "var":
                // hehe, a first type error ... hmm
                line, char = lineFromPos(term.pos)
                throw "'$(term.to)' on line $line is not a mutable"
            rhs = lex(scope, term.value)
            value = {type="op",op=term.op,lhs=ref,{rhs}}
            {type="vset",target=ref,{value}}
        { term.type == "list" }:
            data = (term?data or []).map(term -> lex(scope, term))
            {type="list",{data}}
        { term.type == "object" or term.type == "map" }:
            map = HashMap.new
            fill = Array.new
            (term?data or []).each: entry ->
                { entry?n }:
                    map[entry.n] = null
                    fill.add([entry.n,lex(scope, entry.v)])
                {         }:
                    entry.names.each(term ->
                        map[term.name] = null
                        fill.add([term.name, lex(scope, term)])
                    )
            //if fill.size == 0: return {type="data", data=map.toObject}, true
            {type=term.type, {fill}, map=map.toMap}
        { term.type == "guards" }:
            gs = term.guards.map(guard ->
                cond = lex(scope, (guard?cond or "true"))
                function = lextop(scope,{type="block", body=guard.body, pos=guard.pos})
                _, fn = functions.add({name="",open=false,function=function})
                block = {type="function",function=fn}
                {{cond, block}}
            )
            {type="guards",guards=gs}
        { }:
            print "ERROR"
            print repr(term)
            throw "unknown term $(term.type)"

    emit = term ->
        { term == "undefined" }: ["undef"]
        { term == "null"      }: ["null"]
        { term == "false"     }: ["false"]
        { term == "true"      }: ["true"]
        { isNumber(term) }:
            { isFloat(term) }: ["data", term]
            { term < 0      }: ["data", term]
            { term >= 2^25  }: ["data", term]
            {               }: ["int", term]
        { term.type == "string"   }: ["data", term.data]
        { term.type == "data"     }: ["data", term.data]
        { term.type == "function" }: ["bind", term.function]
        { term.type == "system"   }: ["system", term.name]
        { term.type == "assign"   }: emit(term.value) + ["store", term.to.at - 1]
        { term.type == "vset"     }:
            { term.target.type == "env" }: emit(term.value) + ["evstore", term.target.up - 1, term.target.at - 1]
            {                           }: emit(term.value) + ["vstore", term.target.at - 1]
        { term.type == "multiassign" }:
            v = emit(term.value)
            ls = term.locals.map(l, n -> ["rstore", n - 1, l - 1]).flatten
            as = term.assigns.filter.map(a -> emit(a)).flatten
            v + ls + as
        { term.type == "field" }:
            ["mcall", 0, "this", "data", term.name, "invoke"]
        { term.type == "call" }:
            target = emit(term.target)
            args = (term?args or []).map(arg -> emit(arg))
            if term?block:
                names = term.names.add("block")
                args = args.add(emit(term.block))
                return ["fcalln", args.size, {ns=names}] + target + args + ["invoke"]
            if try(term.names.filter.size > 0):
                return ["fcalln", args.size, {ns=term.names}] + target + args + ["invoke"]
            ["fcall", args.size] + target + args + ["invoke"]
        { term.type == "method" or term.type == "safemethod" }:
            target = emit(term.target)
            args = (term?args or []).map(arg -> emit(arg))
            s = bool(term.type == "safemethod", "s", "")
            if term?block:
                names = term.names.add("block")
                args = args.add(emit(term.block))
                return ["mcalln$s", args.size, {ns=names}] + target + ["data", term.method] + args + ["invoke"]
            if try(term.names.filter.size > 0):
                return ["mcalln$s", args.size, {ns=term.names}] + target + ["data", term.method] + args + ["invoke"]
            ["mcall$s", args.size] + target + ["data", term.method] + args + ["invoke"]
        { term.type == "async" }:
            body = emit(term.body)
            ["fcall", 1, "global", "_Task_new", body, "invoke"]
        { term.type == "not" }:
            rhs = emit(term.rhs)
            ["fcall", 1, "global", "not", rhs, "invoke"]
        { term.type == "~" }:
            rhs = emit(term.rhs)
            ["fcall", 1, "global", "bnot", rhs, "invoke"]
        { term.type == "-" }:
            rhs = emit(term.rhs)
            ["fcall", 1, "global", "neg", rhs, "invoke"]
        { term.type == "op"  }:
            lhs = emit(term.lhs)
            rhs = emit(term.rhs)
            ["bcall", 2, "global", opname(term.op), lhs, rhs, "invoke"]
        { term.type == "guards" }:
            term.guards.map(term ->
                [emit(term.cond), "ccall", 0, emit(term.block), "invoke"]
            ).flatten + ["cerr"]
        { term.type == "local"  }:
            { term?var }: ["vget", term.at - 1]
            {          }: ["local", term.at - 1]
        { term.type == "arg"    }: ["arg", term.at - 1]
        { term.type == "env"    }:
            { term?var }: ["evget", term.up - 1, term.at - 1]
            {          }: ["env", term.up - 1, term.at - 1]
        { term.type == "envarg" }: ["envarg", term.up - 1, term.at - 1]
        { term.type == "global" }: ["global", term.name]
        { term.type == "args"   }: ["args"]
        { term.type == "envargs"}: ["envargs", term.up - 1]
        { term.type == "this"   }: ["this"]
        { term.type == "envthis"}: ["envthis", term.up - 1]
        { term.type == "list" }:
            elements = term.data.map(entry -> emit(entry))
            ["fcall", elements.size, "global", "__list"] + elements + ["invoke"]
        { term.type == "object" }:
            fill = term.fill.map(e -> ["data", e[1]] + emit(e[2]))
            ["fcall", 1 + fill.size * 2, "global", "__object", "data", term.map.toObject] + fill + ["invoke"]
        { term.type == "map" }:
            fill = term.fill.map(e -> ["data", e[1]] + emit(e[2]))
            ["fcall", 1 + fill.size * 2, "global", "__map", "data", term.map.toObject] + fill + ["invoke"]
        { term.type == "class" }:
            ["fcall", 2, "global", "_bless", emit(term.methods), emit(term.class), "invoke"]
        { }:
            print "ERROR"
            print repr(term)
            throw "unknown term $(term?type), $(repr term)"

    emittop = fn, name ->
        code = fn.body.map(term -> emit(term)).flatten
        {{code},name=(fn?name or name or ""),locals=fn.locals,args=fn.args,pos=fn.pos,scope=fn?scope}

    parseemit = text, ast, linkframe ->
        ast = { ast }: ast
              {     }: parse(text)
        ast = lextop(Scope(null), ast, linkframe)
        // TODO wait until stable? as closing functions might produce new functions ...
        functions.each: fn, n -> if fn.open: closefn(n)
        functions.each: fn, n -> if fn.open: closefn(n)
        functions.each: fn, n -> if fn.open: closefn(n)
        //functions.each: fn, n -> print "fn $n:", repr(fn)
        //print "fn 0:", repr(ast)
        functions.each: fn, n -> functions[n] = emittop(fn.function, fn.name)
        main = emittop(ast, "<main>")
        return main, functions

    // cache the position to line information for a little bit
    lines = (
        var $at = 0
        lines = Array.new
        loop:
            $at = text.find('\n', $at + 1)
            if not $at:
                lines.add(text.size)
                break
            if $at: lines.add($at)
        lines
    )

    lineFromPos = pos ->
        var $min = 1
        var $max = lines.size
        while $min < $max:
            mid = ($min + $max) /. 2
            { lines[mid] < pos }: $min = mid + 1
            {                  }: $max = mid
        return $min, pos - $min

    builder_new = functions -> {
        functions = functions
        datas = Array.new
        dataIndex = HashMap.new
        globals = Array.new
        globalsIndex = HashMap.new
        functions = HashMap.new
        data = v, code ->
            if not code:
                ref = encodeLiteral v
                if ref: return ref
            ref = this.dataIndex[v]
            if ref: return ref
            bin = { isList(v)   }: encodeList(v.map(v -> this.data(v)))
                  { isObject(v) }: encodeMap(Object.map(v, (k, v -> [this.data(k), this.data(v)])).flatten)
                  { isString(v) }: encodeString(v)
                  { isFloat(v)  }: encodeFloat(v)
                  { isNumber(v) }: encodeNumber(v)
                  {             }: Error("don't know how to encode: $(repr(v))").throw
            _, at = this.datas.add(bin)
            //print "data", at, repr(v)
            this.dataIndex[v] = encodeInt(at - 1)
        global = v ->
            assert isString(v)
            ref = this.globalsIndex[v]
            if ref: return ref
            ref = this.data(v)
            _, at = this.globals.add(ref)
            this.globalsIndex[v] = encodeInt(at - 1)
        function = name, code, locals, argspecs, debuginfo, scope ->
            code = code.add("end")
            //print "code:", name, debuginfo.line, repr(code)[:20]
            bin = writeCode(this, {{name, locals, argspecs, code, debuginfo}}, scope)
            _, at = this.datas.add(bin)
            //print "function", at, name
            this.functions[name] = encodeInt(at - 1)
        code = v ->
            fn = functions[v]
            return this.function(fn.name, fn.code, fn.locals, fn.args, {line=lineFromPos(fn.pos),text="",offset=0,pos=[]}, fn?scope)
        create = ->
            buf = Buffer.new
            buf.write("tl01")
            buf.write(encodeInt(this.datas.size + 1))
            this.datas[:-2].each: v -> buf.write(v)
            buf.write(encodeList(this.globals))
            buf.write(this.datas[-1])
            return buf
    }

    compilemain = main, function ->
        name = main?name or "<main>"
        locals = main?locals or []
        argspec = main?args or []
        text = main?text or ""
        pos = main?pos or []

        builder = builder_new(functions)
        builder.function(name, main.code, locals, argspec, {line=lineFromPos(main.pos),text=text,offset=0,pos=[]}, main?scope)
        builder.create

    main, functions2 = parseemit(text, ast, args["link"])
    buf = compilemain(main, functions2)
    if args["buffer"]: return buf
    return buf.read
)

print_pos = debugger, stream ->
    pos, text = debugger.pos
    if not pos: return
    begin = (text.find('\n', pos, backward=true) or -1) + 1
    end = (text.find('\n', pos) or text.size + 1) - 1
    stream.write(text, "\n")
    stream.write(" ".times(pos - 1) + "^\n")

SimpleDebugger = debugger, stream ->
    loop:
        //catch: e -> print "oeps", e.toString
        stream.write("INPUT>\n")
        at = stream.find("\n")
        cmd = stream.readString(at).trim
        { cmd == "call" }:
            stream.write(repr(debugger.call), "\n")
        { cmd == "locals" }:
            stream.write(repr(debugger.locals), "\n")
        { cmd == "list" or cmd == "pos" }:
            stream.write(print_pos(debugger, stream), "\n")
        { cmd == "step" or cmd == "" }:
            debugger.step
            print_pos(debugger, stream)
        { cmd == "stop" }:
            debugger.detach(debugger.current)
            debugger.continue
            break

Module = m -> {
        links = -> _module_links(m)
        link = ls -> Module(_module_link(m, ls))
        frame = as ->
            _module_frame(m, as, args["link"])
        run = as, task, frame ->
            _module_run(m, as, task, frame)
        debug = stream, as, frame ->
            debugger = Debugger.new
            task = Task.new
            debugger.attach(task)
            _module_run(m, as, task, frame)
            return SimpleDebugger(debugger, stream)
    }

name_from = envs, name ->
    envs.each: env ->
        { isString(env)   }: Error("cannot link using a String: '$env'").throw
        { isObject(env)   }: x = Object.get(env, name); if isDefined(x): return x
        { isFunction(env) }: x = env(name); if isDefined(x): return x
        { env             }: x = env[name]; if isDefined(x): return x
    return _unknown()

load = bytes ->
    start = time()
    bytes = { isBin(bytes)    }: Buffer.new(bytes)
            { isBuffer(bytes) }: bytes
            {                 }: ast = parse(bytes)
                                 //print "parse", args["name"], (time() - start)
                                 buf = compile(bytes, ast, buffer=true, link=args["link"])
                                 //print "compiled", args["name"], (time() - start)
                                 buf
    m = _Module_new(bytes, args["name"] or "")

    envs = args[2:]
    m = { envs.size > 0 }: links = _module_links(m).map(name -> name_from(envs, name))
                           _module_link(m, links)
        {               }: m

    //print "loaded", args["name"], "took", (time() - start).round, "seconds"
    Module(m)

_parse = text -> parse(text)

{ parse=_parse, { compile, load }, unknown = (-> _unknown())}

