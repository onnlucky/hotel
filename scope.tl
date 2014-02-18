
globals = HashMap.new
all = Array.new
Scope = parent -> {{parent},
    names: HashMap.new
    locals: Array.new
    addOpen: name, function, inMethods ->
        _, all = all.add {{name, inMethods}, open: function, scope: this}
        _, local = this.locals.add(name)
        local = local - 1
        this.names[name] = {{all, local}}
        return local
    add: name ->
        _, all = all.add {{name}}
        _, local = this.locals.add(name)
        local = local - 1
        this.names[name] = {{all, local}}
        return local
    ref: name, level ->
        ref = this.names[name]
        if ref:
            if try(all[ref.all].open): close(ref.all)
            if not level: return ["local", ref.local]
            return ["env", level, ref.local]
        if parent:
            return parent.ref name, 0
        globals[name] = true
        return ["global", name]
}

close = n ->
    ref = all[n]
    all[n] = {name:ref.name}
    emit ref.scope, ref.open, ref.inMethods

emit = scope, term, inMethods ->
    { isNumber(term) }: ["int", term]
    { isString(term) }: ["data", term]
    { term.type == "assign" }:
        type = try(term.type)
        if type == "function" or type == "class" or type == "block":
            local = scope.addOpen(term.to, term.value)
            return ["store", local] // TODO bind bind bind!!!
        local = scope.add(term.to)
        if term.to == "methods" and type == "object":
            emit scope, term.value, true
            return ["store", local]
        v = emit scope, term.value
        return v + ["store", local]
    { term.type == "function" }:
        scope = Scope(scope)
        as = try(term.as) or []
        args = as.map(arg -> scope.add(arg.name);[arg.name])
        code = term.body.map(stm -> emit scope, stm).flatten
        return {{code, args}, locals: scope.locals.toList}
    { term.type == "class" }:
        scope = Scope(scope)
        if try(term.as): term.as.each: arg -> scope.add(arg.name)
        scope.add("this")
        term.body.map: stm -> emit scope, stm
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
        term.data.each: entry ->
            if entry.v.type == "function":
                scope.addOpen(entry.n, entry.v, inMethods)
                return
            scope.add(entry.n)
            emit scope, entry.v, inMethods
    { }:
        print "ERROR"
        print repr(term)
        throw "unknown term $(term.type)"

{
    emit: text ->
        res = emit Scope(null), parse(text)
        all.each: ref, n -> if try(ref.open): close(n, ref)
        return res
}

