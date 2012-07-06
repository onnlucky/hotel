
LocalRef = n, s -> [#local, s]
ScopeRef = n, d, s -> [#scope, d, s]
GlobalRef = n, s -> [#global, s]

Module = { -> {
    globals: Array.new
    globalsIndex: HashMap.new
    data: Array.new
    dataIndex: HashMap.new

    scopeLookup: { name ->
        ref = this.globalsIndex.get name
        if ref: return GlobalRef(name, ref)

        n = this.globals.size
        this.globals.add(name)
        this.globalsIndex.set(name, n)
        assert this.globals.size == this.globalsIndex.size
        return GlobalRef(name, n)
    }
    getData: { datum ->
        ref = this.dataIndex.get(datum)
        if ref: return [#data, ref]

        n = this.data.size
        this.data.add(datum)
        this.dataIndex.set(datum, n)
        assert this.data.size == this.dataIndex.size
        return [#data, n]
    }
    addData: { datum ->
        n = this.data.size
        this.data.add(datum)
        return [#data, n]
    }
}}

Scope = { parent, module -> {
    parent: parent
    module: module

    localnames: Array.new
    locals: HashMap.new
    lookup: { name ->
        ref = this.locals.get name
        if ref: return LocalRef(name, ref)
        this.parent.scopeLookup name, 0
    }
    scopeLookup: { name, depth ->
        ref = this.locals.get name
        if ref: return ScopeRef(name, ref)
        this.parent.scopeLookup name, depth + 1
    }
    bind: { name ->
        print "bind", name
        n = this.locals.size
        this.locals.set(name, n)
        this.localnames.add(name)
        assert this.localnames.get(n) == name
        return n
    }
}}

isCall = { what -> return what.type == #call }
isSend = { what -> return what.type == #send }
isBind = { what -> return what.type == #assign }

isFunc = { what -> return what.type == #fn }

emit = { scope, what ->
    print "EMIT:", what
    if isSym what: return scope.lookup what
    if isNumber what: {
        if what > 0 and what < 100: return [#int, what]
        return scope.module.getData(what)
    }
    if isText what: return scope.module.getData(what)
    if isCall what: return emitCall scope, what
    if isSend what: return emitSend scope, what
    if isBind what: return emitBind scope, what

    if isFunc what: return emitFunc scope, what
    _throw "missing ... $(inspect what)"
}
emitBind = { scope, bind ->
    b = scope.bind bind.name
    return [#bind, b]
}
emitCall = { scope, call ->
    target = emit scope, call.target
    args = call.args.map: e -> emit scope, e
    return [#drop, #call, args.size, target].cat(args)
}

emitSend = { scope, send ->
    target = emit scope, send.target
    args = send.args.map: e -> emit scope, e
    msg = scope.module.getData(send.msg)
    return [#drop, #send, args.size, target, msg].cat(args)
}

emitFunc = { parent, func ->
    print "FUNC", inspect(func)
    scope = Scope(parent, parent.module)

    names = Array.new
    defaults = HashMap.new
    args = func.args.map: { arg ->
        index = scope.bind(arg.name)
        names.add(arg.name)
        assert index == names.size - 1
        if arg?default: defaults.set(arg.name, arg.default)
        [arg.type, index]
    }
    body = func.body.map: expr -> emit scope, expr
    code = args.cat([#endargs]).cat(body).flatten
    fn = {
        locals: scope.localnames.toList
        defaults: null
        name: "<anon>"
        code: code
    }
    return scope.module.addData(fn)
}

{ mod ->
    print inspect mod
    module = Module()
    scope = Scope(module, module)

    body = mod.map: expr -> emit scope, expr
    code = [#endargs].cat(body).flatten

    mod = {
        data: module.data.toList
        body: code
        globals: module.globals.toList
        name: "<unknown>"
    }

    print inspect mod
    return mod
}

