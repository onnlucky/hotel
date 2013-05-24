
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
    print "FUNC", inspect(func)
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

( mod ->
    print inspect mod
    module = Module()
    scope = Scope(module, module)

    body = mod.map: expr -> emit scope, expr
    code = body.cat([#end]).flatten

    mod = {
        data: module.data.toList
        body: code
        globals: module.globals.toList
        name: "<unknown>"
    }

    print inspect mod
    return mod
)
