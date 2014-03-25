
globals = HashMap.new
functions = Array.new
Scope = parent -> {{parent},
    names: HashMap.new
    locals: Array.new
    addOpen: name, function, inMethods ->
        _, fn = functions.add {{name, inMethods}, open: function, scope: this}
        _, local = this.locals.add(name)
        local = local - 1
        this.names[name] = {{fn, local}}
        return local, fn
    add: name ->
        _, local = this.locals.add(name)
        local = local - 1
        this.names[name] = {{local}}
        return local
    ref: name, level ->
        ref = this.names[name]
        if try(ref.fn):
            if functions[ref.fn].open: close(ref.fn)
        if ref:
            if not level: return ["local", ref.local]
            return ["env", level, ref.local]
        if parent:
            return parent.ref name, 0
        globals[name] = true
        return ["global", name]
}

close = n ->
    ref = functions[n]
    functions[n] = {name:ref.name,open:false}
    fn = emittop ref.scope, ref.open, ref.inMethods
    functions[n] = {name:ref.name,open:false,function:fn}


emittop = scope, term, inMethods ->
    print "TOP TERM", repr(term)
    { term.type == "function" }:
        scope = Scope(scope)
        as = try(term.as) or []
        args = as.map(arg -> scope.add(arg.name); [arg.name])
        code = term.body.map(stm -> emit scope, stm).flatten
        return {{code, args}, locals: scope.locals.toList}
    { term.type == "class" }:
        scope = Scope(scope)
        as = try(term.as) or []
        args = as.map(arg -> scope.add(arg.name); [arg.name])
        local = scope.add("this")
        code = term.body.map(stm -> emit scope, stm).flatten
        return {{code, args}, locals: scope.locals.toList}

emit = scope, term, inMethods ->
    print "SUB TERM", repr(term)
    { isNumber(term) }: ["int", term]
    { isString(term) }: ["data", term]
    { term.type == "function" or term.type == "class" or term.type == "block" }:
        function = emittop scope, term, inMethods
        _, fn = functions.add {name:"-", open:false, function:function}
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
        args = args.map: stm -> emit scope, stm.v
        ["fcall", args.size] + fn + args + ["invoke"]
    { term.type == "ref" }:
        scope.ref(term.name)
    { term.type == "method" }:
        emit scope, term.target
        term.args.each: stm -> emit scope, stm.v
    { term.type == "object" }:
        elements = term.data.map(entry ->
            if try(entry.v.type) == "function":
                return ["data", entry.n, scope.addOpen(entry.n, entry.v, inMethods)]
            scope.add(entry.n)
            ["data", entry.n, emit(scope, entry.v, inMethods)]
        )
        ["fcall", elements.size * 2, "global", "__map"] + elements + ["invoke"]
    { term.type == "list" }:
        elements = term.data.map(entry -> [emit(scope, entry)])
        ["fcall", elements.size, "global", "__list"] + elements + ["invoke"]
    { }:
        print "ERROR"
        print repr(term)
        throw "unknown term $(term.type)"

{
    emit: text ->
        globals.clear; functions.clear // TODO come on, make this a real module/object thing
        res = emittop Scope(null), parse(text)
        functions.each: fn, n -> if fn.open: close(n)
        functions.each: fn, n -> print "fn $n:", repr(fn)
        print "main:", repr(res)
        return res, functions
}

