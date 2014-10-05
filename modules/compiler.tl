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

encodeNumber = n ->
    pos = n >= 0
    n = n.abs
    if n < 1 <<   7: return Bin(0b1100_0000 | 0,                           n << 1 | bool(pos,0,1))
    if n < 1 <<  15: return Bin(0b1100_0000 | 1,                   n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  23: return Bin(0b1100_0000 | 2,          n >> 15, n >> 7, n << 1 | bool(pos,0,1))
    if n < 1 <<  31: return Bin(0b1100_0000 | 3, n >> 23, n >> 15, n >> 7, n << 1 | bool(pos,0,1))
    assert false

encodeString = s ->
    assert isString s
    if s.size < 32: return Bin(0b0000_0000 | s.size, s)
    return Bin(0b1110_0000, encodeInt(s.size), s)

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
            buf.write mod.data(c.code[($pc += 1)], true)
        { op == "global" }: // a reference to the linked list (a string)
            buf.write 0b1100_0000 + 7
            buf.write mod.global(c.code[($pc += 1)])
        { op == "bind"   }: // bind a body into a closure, references bytecode stored in module data
            buf.write 0b1100_0000 + 16
            buf.write mod.code(c.code[($pc += 1)])
        { op == "true"   }: buf.write 0b1100_0000 + 0
        { op == "false"  }: buf.write 0b1100_0000 + 1
        { op == "null"   }: buf.write 0b1100_0000 + 2
        { op == "undef"  }: buf.write 0b1100_0000 + 3
        { op == "int"    }: buf.write 0b1100_0000 + 4; buf.write encodeInt c.code[($pc += 1)]
        { op == "envarg" }: buf.write 0b1100_0000 + 8; buf.write encodeInt c.code[($pc += 1)]; buf.write encodeInt c.code[($pc += 1)]
        { op == "env"    }: buf.write 0b1100_0000 + 9; buf.write encodeInt c.code[($pc += 1)]; buf.write encodeInt c.code[($pc += 1)]
        { op == "arg"    }: buf.write 0b1100_0000 + 10; buf.write encodeInt c.code[($pc += 1)]
        { op == "local"  }: buf.write 0b1100_0000 + 11; buf.write encodeInt c.code[($pc += 1)]
        { op == "args"   }: buf.write 0b1100_0000 + 12
        { op == "envargs"}: buf.write 0b1100_0000 + 13; buf.write encodeInt c.code[($pc += 1)]
        { op == "this"   }: buf.write 0b1100_0000 + 14
        { op == "envthis"}: buf.write 0b1100_0000 + 15; buf.write encodeInt c.code[($pc += 1)]

        { op == "store"  }: buf.write 0b1100_0000 + 17; buf.write encodeInt c.code[($pc += 1)]
        { op == "rstore" }: buf.write 0b1100_0000 + 18; buf.write encodeInt c.code[($pc += 1)]; buf.write encodeInt c.code[($pc += 1)]
        { op == "invoke" }: buf.write 0b1100_0000 + 19

        { op == "mcall"  }: buf.write 0b1110_0000 + 0; buf.write encodeInt c.code[($pc += 1)]
        { op == "mcalls" }: buf.write 0b1110_1000 + 0; buf.write encodeInt c.code[($pc += 1)]
        { op == "fcall"  }: buf.write 0b1110_0000 + 1; buf.write encodeInt c.code[($pc += 1)]
        { op == "bcall"  }: buf.write 0b1110_0000 + 2; buf.write encodeInt c.code[($pc += 1)]
        { op == "mcalln" }: buf.write 0b1111_0000 + 0; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)
        { op == "mcallns"}: buf.write 0b1111_1000 + 0; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)
        { op == "fcalln" }: buf.write 0b1111_0000 + 1; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)
        { op == "bcalln" }: buf.write 0b1111_0000 + 2; buf.write encodeInt c.code[($pc += 1)]; buf.write mod.data(c.code[($pc += 1)].ns)

        {                }: throw "unknown op: $op $(repr(op))"

    if buf.size < 32: return Bin(0b1010_0000 | buf.size, buf.read)
    return Bin(0b1110_0101, encodeInt(buf.size), buf)

opname = op ->
    { op == "or"  }: op
    { op == "and" }: op
    { op == "not" }: op

    { op == "==" }: return "eq"
    { op == "!=" }: return "neq"

    { op == ">" }: return "gt"
    { op == ">=" }: return "gte"
    { op == "<" }: return "lt"
    { op == "<=" }: return "lte"

    { op == "|" }: return "bor"
    { op == "&" }: return "band"
    { op == "<<" }: return "lsh"
    { op == ">>" }: return "rsh"

    { op == "+" }: return "add"
    { op == "-" }: return "sub"
    { op == "*" }: return "mul"
    { op == "/" }: return "div"
    { op == "/."}: return "idiv"
    { op == "%" }: return "mod"
    { op == "^" }: return "pow"

compile = (text, ast ->
    globals = HashMap.new
    functions = Array.new
    ids = Array.new

    Scope = parent, block -> {{parent},
        parentVisible = try(parent.locals.size, 0)
        block = block or false
        names = HashMap.new
        locals = Array.new
        args = Array.new
        // TODO addOpen vs add, when function is named is clumsy, either collapse at assign, or do this
        addOpen = function, name, pos ->
            _, fn = functions.add {name=name or "",open=function,scope=this}
            if name:
                _, id = ids.add({{name},at=Array.new([pos])})
                _, at = this.locals.add(name)
                this.names[name] = {arg=false,{at,id,fn}}
            return fn
        add = name, pos ->
            if this.locals[-1] == name and try(this.names[name].fn):
                ref = this.names[name]
                return ref.at, ref.id
            _, id = ids.add({{name},at=Array.new([pos])})
            _, at = this.locals.add(name)
            this.names[name] = {arg=false,{at,id}}
            return at, id
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
            if try(ref.fn):
                if functions[ref.fn].open: close(ref.fn)
            if ref:
                if ref.arg:
                    if not level: return {type="arg",at=ref.at,id=ref.id}
                    return {type="envarg",up=level,at=ref.at,id=ref.id}
                if not level: return {type="local",at=ref.at,id=ref.id}
                return {type="env",up=level,at=ref.at,id=ref.id}
            if name == "args" and not this.block:
                if not level: return {type="args"}
                return {type="envargs",up=level}
            if name == "this" and not this.block:
                if not level: return {type="this"}
                return {type="envthis",up=level}
            if parent:
                if not level: return parent.ref name, 1
                return parent.ref name, level + 1
            globals[name] = true
            return {type="global",{name}}
    }

    ClassScope = parent -> {{parent},
        names = HashMap.new
        locals = Array.new
        methods = Array.new
        addOpen = function, name ->
            _, method = functions.add {name=name or "",open=function,scope=this}
            if name:
                // TODO perhaps change type="function" to type="method"
                ref = this.names[name]
                assert ref and ref.at == this.locals.size
                this.locals.remove(ref.at)
                this.names[name] = {arg=false,id=ref.id,{method}}
            return method
        add = name, pos ->
            _, id = ids.add({{name},at=Array.new([pos])})
            _, at = this.locals.add(name)
            this.names[name] = {arg=false,{at,id}}
            return at, id
        addarg = name, pos ->
            _, id = ids.add({{name},at=Array.new([pos])})
            this.names[name] = {arg=true,{id}}
            return id
        ref = name, level ->
            ref = this.names[name]
            if ref:
                if not level: return {type="local",at=ref.at,id=ref.id}
                return {type="field",name=name}
            if name == "args":
                if not level: return {type="args"}
                return {type="thisargs",up=level}
            if not level: return parent.ref name, 0
            parent.ref name, level + 1
    }

    close = n ->
        ref = functions[n]
        functions[n] = {name=ref.name,open=false}
        fn = lextop(ref.scope, ref.open)
        functions[n] = {name=ref.name,open=false,function=fn}

    argspec = arg ->
        if arg.type == "lazy": return [arg.name, arg.d, true]
        if arg.d: return [arg.name, arg.d]
        [arg.name]

    lextop = scope, term ->
        { term.type == "function" }:
            scope = Scope(scope)
            args = try(term.as, []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            {type="function",{args,body},locals=scope.locals.toList,pos=term.pos}
        { term.type == "block" }:
            scope = Scope(scope, true)
            args = try(term.as, []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            {type="block",{args,body},locals=scope.locals.toList,pos=term.pos}
        { term.type == "scope" }:
            scope = Scope(scope, true)
            args = try(term.as, []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            {type="scope",{args,body},locals=scope.locals.toList,pos=term.pos}

    lex = scope, term, name, pos ->
        { term == null              }: "null"
        { isChar(term)              }: {type="call",target={type="global",name="Char"},args=[term.toNumber]}
        { not try(term.type)        }: term
        { term.type == "string"     }: term
        { term.type == "stringlist" }:
            data = term.data.map(s ->
                { isString(s) }: {type="string",data=s}
                {             }: lex(scope, s)
            )
            {type="call",target={type="global",name="_String_cat"},args=data}
        { term.type == "scope" }:
            function = lextop scope, term
            _, fn = functions.add {name="",open=false,function=function}
            {type="call",target={type="function",function=fn}}
        { term.type == "function" or term.type == "block" }:
            function = scope.addOpen(term, name, pos)
            if not name: close(function)
            {type="function",{function}}
        { term.type == "class" }:
            outerscope = scope
            local = scope.add("", pos)
            scope = ClassScope(scope)
            args = try(term.as, []).map(arg, n -> scope.addarg(arg.name, arg.pos); argspec(arg))
            body = term.body.map(stm -> lex(scope, stm))
            class = body.filter(stm -> try(stm.type == "assign" and stm.value.type == "function")).map(stm, i -> {n=stm.to.name,v=stm.value})
            constructor = body.filter(stm -> not try(stm.type == "assign" and stm.value.type == "function"))
            fields = scope.locals.map(name -> {n=name,v=scope.ref(name)})
            print "---- class: $class ----"
            print repr class
            print repr constructor
            print repr fields
            print "---- class: $class ----"
            {type="class",{class,constructor,fields,name,local}}
        { term.type == "assign" }:
            { term.to.type == "local" }:
                value = lex(scope, term.value, term.to.name, term.to.pos)
                local, id = scope.add(term.to.name, term.to.pos)
                to={type="local",name=term.to.name,pos=term.to.pos,at=local,{id}}
                {type="assign",{to,value}}
            { term.to.type == "newvar" }:
                local, id = scope.add(term.to.name, term.to.pos)
                value = lex(scope, term.value)
                value = {type="method",target={type="global",name="Var"},method="new",args=[value]}
                to={type="local",name=term.to.name,pos=term.to.pos,at=local,{id}}
                {type="assign",{to,value}}
            { term.to.type == "var"    }:
                ref = scope.ref(term.to.name)
                value = lex(scope, term.value)
                {type="method",target=ref,method="set",args=[value]}
        { term.type == "multiassign" }:
            // TODO perhaps function defined this way must stay open?
            value = lex(scope, term.value)
            locals = term.multi.map(to ->
                { to.type == "local" }: scope.add(to.name, to.pos)
                {                    }: scope.add("", to.pos) // anonymous locals for Var.new and var.set
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
            names = try(term.args, []).map: stm -> try(stm.n)
            args = try(term.args, []).map: stm -> lex(scope, stm.v)
            block = try(lex(scope, term.block))
            if try(target.type) == "return" or try(target.type) == "goto":
                t = {type="global",name=target.type}
                return {type="call",target=t,args=[target.scope]+args,names=[null]+names,{block}}
            if try(target.type) == "break" or try(target.type) == "continue":
                t = {type="global",name=target.type}
                s = lex(scope, {type="list",data=target.scope})
                return {type="call",target=t,args=[s]+args,names=[null]+names,{block}}
            {type="call",{target,args,names,block}}
        { term.type == "method" or term.type == "safemethod" }:
            target = lex(scope, term.target)
            names = try(term.args, []).map: stm -> try(stm.n)
            args = try(term.args, []).map: stm -> lex(scope, stm.v)
            block = try(lex(scope, term.block))
            {type=term.type,method=term.method,{target,args,names,block}}
        { term.type == "async" }:
            body = { try(term.body) }: term.body
                   {                }: [term.rhs]
            function = lextop(scope, {type="scope",{body},pos=term.pos})
            _, fn = functions.add {name="",open=false,function=function}
            {type="async",body={type="function",function=fn}}
        { term.type == "not" }:
            rhs = lex(scope, term.rhs)
            {type="not",{rhs}}
        { term.type == "op"  }:
            lhs = lex(scope, term.lhs)
            rhs = lex(scope, term.rhs)
            {type="op",op=term.op,{lhs,rhs}}
        { term.type == "ref" }:
            scope.ref(term.name)
        { term.type == "varref" }:
            ref = scope.ref(term.name)
            {type="method",method="get",target=ref,args=[]}
        { term.type == "assignop" }:
            ref = scope.ref(term.to)
            lhs = {type="method",method="get",target=ref,args=[]}
            rhs = lex(scope, term.value)
            value = {type="op",op=term.op,{lhs,rhs}}
            {type="method",target=ref,method="set",args=[value]}
        { term.type == "object" }:
            data = try(term.data, []).map(entry ->
                { try(entry.n) }: {n=entry.n,v=lex(scope, entry.v)}
                {              }: entry.names.map(term -> {n=term.name,v=lex(scope, term)})
            ).flatten
            {type="object",{data}}
        { term.type == "list" }:
            data = try(term.data, []).map(term -> lex(scope, term))
            {type="list",{data}}
        { term.type == "map" }:
            data = try(term.data, []).map(entry ->
                { try(entry.n) }: {n=entry.n,v=lex(scope, entry.v)}
                {              }: entry.names.map(term -> {n=term.name,v=lex(scope, term)})
            ).flatten
            {type="map",{data}}
        { term.type == "guards" }:
            scope = Scope(scope, true)
            target = {type="global",name="_match"}
            gs = term.guards.map(guard ->
                args = [lex(scope, try(guard.cond, "true"))]
                names =[null]
                function = lextop(scope,{type="block", body=guard.body, pos=guard.pos})
                _, fn = functions.add {name="",open=false,function=function}
                block = {type="function",function=fn}
                {type="call",{target,args,names,block}}
            )
            function = {body=gs + [{type="call",target={type="global",name="_nomatch"},args=[]}],locals=[],args=[], pos=term.pos}
            _, fn = functions.add {name="", open=false, function=function}
            target = {type="function",function=fn}
            {type="call",{target},args=[],names=[],block=null}
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
            { term > 2^25   }: ["data", term]
            {               }: ["int", term]
        { term.type == "string"   }: ["data", term.data]
        { term.type == "function" }: ["bind", term.function]
        { term.type == "assign"   }: emit(term.value) + ["store", term.to.at - 1]
        { term.type == "multiassign" }:
            v = emit(term.value)
            ls = term.locals.map(l, n -> ["rstore", n - 1, l - 1]).flatten
            as = term.assigns.filter.map(a -> emit(a)).flatten
            v + ls + as
        { term.type == "call" }:
            target = emit(term.target)
            args = try(term.args,[]).map(arg -> emit(arg))
            if try(term.block):
                names = term.names.add("block")
                args = args.add(emit(term.block))
                return ["fcalln", args.size, {ns=names}] + target + args + ["invoke"]
            if try(term.names.filter.size > 0):
                return ["fcalln", args.size, {ns=term.names}] + target + args + ["invoke"]
            ["fcall", args.size] + target + args + ["invoke"]
        { term.type == "method" or term.type == "safemethod" }:
            target = emit(term.target)
            args = try(term.args,[]).map(arg -> emit(arg))
            s = bool(term.type == "safemethod", "s", "")
            if try(term.block):
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
        { term.type == "op"  }:
            lhs = emit(term.lhs)
            rhs = emit(term.rhs)
            ["bcall", 2, "global", opname(term.op), lhs, rhs, "invoke"]
        { term.type == "local"  }: ["local", term.at - 1]
        { term.type == "arg"    }: ["arg", term.at - 1]
        { term.type == "env"    }: ["env", term.up - 1, term.at - 1]
        { term.type == "envarg" }: ["envarg", term.up - 1, term.at - 1]
        { term.type == "global" }: ["global", term.name]
        { term.type == "args"   }: ["args"]
        { term.type == "envargs"}: ["envargs", term.up - 1]
        { term.type == "this"   }: ["this"]
        { term.type == "envthis"}: ["envthis", term.up - 1]
        { term.type == "object" }:
            elements = term.data.map(entry -> ["data", entry.n] + emit(entry.v))
            ["fcall", elements.size * 2, "global", "__object"] + elements + ["invoke"]
        { term.type == "list" }:
            elements = term.data.map(entry -> emit(entry))
            ["fcall", elements.size, "global", "__list"] + elements + ["invoke"]
        { term.type == "map" }:
            elements = term.data.map(entry -> ["data", entry.n] + emit(entry.v))
            ["fcall", elements.size * 2, "global", "__map"] + elements + ["invoke"]
        { }:
            print "ERROR"
            print repr(term)
            throw "unknown term $(term.type)"

    emittop = fn, name ->
        code = fn.body.map(term -> emit(term)).flatten
        {{code},name=try(fn.name, name or ""),locals=fn.locals,args=fn.args,pos=fn.pos}

    parseemit = text, ast ->
        ast = { ast }: ast
              {     }: parse(text)
        ast = lextop(Scope(null), ast)
        // TODO wait until stable? as closing functions might produce new functions ...
        functions.each: fn, n -> if fn.open: close(n)
        functions.each: fn, n -> if fn.open: close(n)
        functions.each: fn, n -> if fn.open: close(n)
        //functions.each: fn, n -> print "fn $n:", repr(fn)
        //print "fn 0:", repr(ast)
        functions.each: fn, n -> functions[n] = emittop(fn.function, fn.name)
        main = emittop(ast, "<main>")
        return main, functions

    // cache the position to line information for a little bit
    at = Var.new 0
    lines = Var.new 1
    lineFromPos = pos ->
        if pos < 0: return 0
        assert pos <= text.size
        if pos < $at: $at = 0; $lines = 1
        while $at < pos:
            if text[$at] == '\n': $lines += 1
            $at += 1
        $lines

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
            bin = { isList(v)   }: encodeList v.map(v -> this.data v)
                  { isObject(v) }: encodeMap Object.map(v, (k, v -> [this.data(k), this.data(v)])).flatten
                  { isString(v) }: encodeString v
                  { isFloat(v)  }: encodeFloat v
                  { isNumber(v) }: encodeNumber v
            _, at = this.datas.add bin
            //print "data", at, repr(v)
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
            //print "code:", name, debuginfo.line, repr(code)[:20]
            bin = writeCode this, {{name, locals, argspecs, code, debuginfo}}
            _, at = this.datas.add bin
            //print "function", at, name
            this.functions[name] = encodeInt(at - 1)
        code = v ->
            fn = functions[v]
            return this.function(fn.name, fn.code, fn.locals, fn.args, {line=lineFromPos(fn.pos),text="",offset=0,pos=[]})
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
        name = try(main.name, "<main>")
        locals = try(main.locals, [])
        argspec = try(main.args, [])
        text = try(main.text, "")
        pos = try(main.pos, [])

        builder = builder_new(functions)
        builder.function(name, main.code, locals, argspec, {line=lineFromPos(main.pos),text=text,offset=0,pos=[]})
        builder.create

    main, functions = parseemit(text, ast)
    buf = compilemain(main, functions)
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
            debugger.detach debugger.current
            debugger.continue
            break

Module = m -> {
        links = -> _module_links(m)
        link = ls -> Module(_module_link(m, ls))
        run = as, am, task ->
            _module_run(m, task, as, am)
        debug = stream, as, am ->
            debugger = Debugger.new
            task = Task.new
            debugger.attach(task)
            _module_run(m, task, as, am)
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
                                 buf = compile(bytes, ast, buffer=true)
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

