p = parse("{->methods={test=->42}\nprint test}")

globals = HashMap.new
all = Array.new
Scope = parent -> {{parent},
    names: HashMap.new
    addOpen: name, function, inMethods ->
        all.add {{name, inMethods}, open: function, scope: this}
        this.names[name] = all.size
    add: name ->
        all.add {{name}}
        this.names[name] = all.size
    ref: name ->
        ref = this.names[name]
        if ref:
            if all[ref].open: close(ref)
            return
        if parent:
            return parent.ref name
        globals[name] = true
}

close = n ->
    ref = all[n]
    all[n] = {name:ref.name}
    emit ref.scope, ref.open, ref.inMethods

emit = scope, term, inMethods ->
    { isNumber(term) }: true //print "number", term
    { isString(term) }: true //print "string", term
    { term.type == "assign" }:
        if term.value.type == "function" or term.value.type == "class" or term.value.type == "block":
            scope.addOpen(term.to, term.value)
            return
        scope.add(term.to)
        if term.to == "methods" and term.value.type == "object":
            return emit scope, term.value, true
        emit scope, term.value
    { term.type == "function" }:
        scope = Scope(scope)
        if term.as: term.as.each: arg -> scope.add(arg.name)
        term.body.each: stm -> emit scope, stm
    { term.type == "class" }:
        scope = Scope(scope)
        if term.as: term.as.each: arg -> scope.add(arg.name)
        scope.add("this")
        term.body.each: stm -> emit scope, stm
    { term.type == "call" }:
        emit scope, term.target
        term.args.each: stm -> emit scope, stm.v
    { term.type == "ref" }:
        item = scope.ref(term.name)
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

emit Scope(null), p
all.each: ref, n -> if ref.open: close(n, ref)

print "---- locals:"
all.each: ref, n ->
    assert not ref.open
    print n, ref.name

print "---- globals:"
globals.each: k, v -> print k
print "----"
