p = parse("{->methods={test=->42}\nprint test}")

globals = HashMap.new
all = Array.new
Scope = { parent ->
    names = HashMap.new

    addOpen = name, function ->
        all.add({{name}, open=function, scope=this})
        this.names[name] = all.size
    add = name ->
        all.add({{name}})
        this.names[name] = all.size
    ref = name ->
        ref = this.names[name]
        { ref    }: close(ref)
        { parent }: parent.ref(name)
        {        }: globals[name] = true
}

close = n ->
    ref = all[n]
    if ref.open:
        all[n] = {name=ref.name}
        emit ref.scope, ref.open

emit = scope, term ->
    { isNumber(term) }:
    { isString(term) }:
    { term.type == "assign" and function_like[term.value.type] }:
        scope.addOpen(term.to, term.value)
    { term.type == "assign" }:
        scope.add(term.to)
        emit scope, term.value
    { term.type == "function" }:
        scope = Scope(scope)
        term.args?each: arg -> scope.add(arg.name)
        term.body.each: stm -> emit scope, stm
    { term.type == "class" }:
        scope = Scope(scope)
        term.args?each: arg -> scope.add(arg.name)
        scope.add("this")
        term.body.each: stm -> emit scope, stm
    { term.type == "ref" }:
        item = scope.ref(term.name)
    { term.type == "call" }:
        emit scope, term.target
        term.args.each: stm -> emit scope, stm.v
    { term.type == "method" }:
        emit scope, term.target
        term.args.each: stm -> emit scope, stm.v
    { term.type == "object" }:
        term.data.each: entry ->
            { function_like(entry.v.type) }:
                scope.addOpen(entry.n, entry.v)
            { }:
                scope.add(entry.n)
                emit scope, entry.v
        term.data.each: entry -> scope.ref(entry.n)
    { }:
        throw "unknown term $(term.type) $(rerp(term))"

emit Scope(null), p
all.each: ref, n -> if ref.open: close(n, ref)

print "---- locals:"
all.each: ref, n ->
    assert not ref.open
    print n, ref.name

print "---- globals:"
globals.each: k, v -> print k
print "----"

