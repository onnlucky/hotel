p = parse("even = -> odd();even()\nprint()\nodd = -> even(); odd()\noddxx()")

globals = HashMap.new
all = Array.new
Scope = parent -> {{parent},
    names: HashMap.new
    addOpen: name, function ->
        all.add {{name}, open: function, scope: this}
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
    emit ref.scope, ref.open

emit = scope, term ->
    { isNumber(term) }: true //print "number", term
    { isString(term) }: true //print "string", term
    { term.type == "assign" }:
        if term.value.type == "function":
            scope.addOpen(term.to, term.value)
            return
        scope.add(term.to)
        emit scope, term.value
    { term.type == "function" }:
        scope = Scope(scope)
        if term.as: term.as.each: arg -> scope.add(arg.name)
        term.body.each: stm -> emit scope, stm
    { term.type == "call" }:
        emit scope, term.target
        term.args.each: stm -> emit scope, stm.v
    { term.type == "ref" }:
        item = scope.ref(term.name)
    { term.type == "method" }:
        emit scope, term.target
        term.args.each: stm -> emit scope, stm.v

emit Scope(null), p
all.each: ref, n -> if ref.open: close(n, ref)

print "---- locals:"
all.each: ref, n ->
    assert not ref.open
    print n, ref.name

print "---- globals:"
globals.each: k, v -> print k
print "----"
