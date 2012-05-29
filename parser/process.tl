
LocalRef = n, s -> [#local, s]
ScopeRef = n, d, s -> [#scope, d, s]
GlobalRef = n, s -> [#global, s]

GlobalScope = { -> {
    globals: HashMap.new
    slookup: { name ->
        ref = this.globals.get name
        if ref: return GlobalRef(name, ref)

        n = this.globals.size
        this.globals.set(name, n)
        return GlobalRef(name, n)
    }
}}

Scope = { parent -> {
    parent: parent
    locals: HashMap.new

    lookup: { name ->
        ref = this.locals.get name
        if ref: return LocalRef(name, ref)
        this.parent.slookup name, 0
    }
    slookup: { name, depth ->
        ref = this.locals.get name
        if ref: return ScopeRef(name, ref)
        this.parent.slookup name, depth + 1
    }
    bind: { name ->
        n = this.locals.size
        this.locals.set(name, n)
        return [0, n]
    }
}}

isCall = { what -> return what.type == #call }
isSend = { what -> return what.type == #send }
isBody = { what -> return what.type == #body }

emit = { scope, what ->
    print "EMIT:", what
    if isSym what: return scope.lookup what
    if isNumber what: return [#data, what]
    if isText what: return [#data, what]
    if isCall what: return emitCall scope, what
    if isSend what: return emitSend scope, what
    if isBody what: return emitBody scope, what
}
emitCall = { scope, call ->
    target = emit scope, call.target
    args = call.args.map: e -> emit scope, e
    return [#drop, #call, args.size, target].cat(args)
}

emitSend = { scope, send ->
    target = emit scope, send.target
    args = send.args.map: e -> emit scope, e
    return [#drop, #send, args.size, target, send.msg].cat(args)
}

emitBody = { scope, body ->
    scope = Scope(scope)
    body.map: expr -> emit scope, expr
}

{ body ->
    scope = GlobalScope()
    b = emitBody scope, body
    scope.globals.keys.each: k -> print "GLOBALS:", k
    return b.flatten
}

