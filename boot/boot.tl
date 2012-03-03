// boot.tl
// 1. initialize into full std lib (some of it is in controlflow.c)
// 2. run the requests file(s) or start a repl
// 3. drop into the io runloop, exiting when no tasks are left

// TODO print false or true -> is error because false becomes function call (parser error)
or = { left, &right ->
    if not left: return right()
    return left
}
and = { left, &right ->
    if left: return right()
    return left
}
while = { &cond ->
    block = args.block; if not block: _throw "while expects a block"
    loop: {
        if not cond(): break
        block()
    }
}
try = { ->
    if not args.block: _throw "try expects a block"
    _catch: return null
    args.block.call()
}
_each = { fn ->
    block = (args.block) or fn
    if not block: _throw "each expects a block or fn"
    len = this.size
    loop = { at =>
        if at >= len: return
        block(this.get(at))
        goto(loop(at + 1))
    }
    loop(0)
}

// TODO should have special boot places for text, list and such
_install(Text.class, #each, _each)
_install(Text.class, #split, (split = { on ->
    on = (on) or " " // TODO this is weird in the parser; hence the braces
    onlen = on.size
    loop = { start, at, list ->
        if this.startsWith(on, at): {
            goto(loop(at + onlen, at + onlen, list.add(this.slice(start, at))))
        }
        if this.size <= at: {
            return list.add(this.slice(start, at))
        }
        goto(loop(start, at + 1, list))
    }
    return loop(0, 0, [])
}))
_install(Set.class, #each, _each)
_install(List.class, #each, _each)
_install(List.class, #join, (join = { sep ->
    if this.size == 0: return ""
    sep = (sep) or ""

    var $res = this.get(0).toText
    var $i = 1
    while $i < this.size: {
        $res = $res.cat(sep).cat(this.get($i).toText)
        $i += 1
    }
    return $res
}))
_install(List.class, #reduce, (reduce = { fn ->
    block = (args.block) or fn
    if not block: _throw "reduce expects a block or fn"
    if this.size == 0: return null
    if this.size == 1: return this.get(0)

    var $i = 1
    var $res = this.get(0)
    while $i < this.size: {
        $res = block($res, this.get($i))
        $i += 1
    }
    return $res
}))
_install(List.class, #map, (map = { fn ->
    block = (args.block) or fn
    if not block: _throw "map expects a block or fn"
    if this.size == 0: return this

    res = _List_unsafe(this.size)

    var $i = 0
    while $i < this.size: {
        _list_set(res, $i, block(this.get($i)))
        $i += 1
    }
}))

inspect = { x ->
    toJSON = { x, indent, buf, inlist -> (
        _match(isBool(x)): buf.write(x); return false
        _match(not x): buf.write("null"); return false
        _match(isNumber(x)): buf.write(x); return false
        _match(isSym(x)): buf.write("#"); buf.write(x.toText.escape); return false
        _match(isText(x)): buf.write("\""); buf.write(x.escape); buf.write("\""); return false
        _match(isList(x)): (
            if x.size == 0: buf.write("[]"); return false
            if indent.size > 0: buf.write("\n")
            x.each: { v ->
                buf.write(indent); buf.write("- ");
                nl = toJSON(v, indent.cat(" "), buf, true);
                if not nl: buf.write("\n")
            }
            return true
        )
        _match(isObject(x)): (
            keys = _Map_keys(x)
            if keys.size == 0: buf.write("{}"); return false
            if (not inlist and indent.size > 0): buf.write("\n")
            indent = bool(inlist, indent.cat(" "), indent)
            var $first = true
            keys.each: { key =>
                if not inlist: buf.write(indent)
                if inlist: if not $first: buf.write(indent)
                buf.write(key.toText); buf.write(": ")
                nl = toJSON(_Map_get(x, key), indent.cat(" "), buf)
                if not nl: buf.write("\n")
                $first = false
            }
            return true
        )
        _nomatch()
    )}
    buf = io.Buffer.new
    toJSON(x, "", buf)
    buf.read
}

// TODO should not sleep in ms, but need float ... should also add Time module
sleep =   ms -> io.wait ms
seconds = s -> s * 1000
minutes = m -> m * 1000 * 60
hours   = h -> m * 1000 * 60 * 60
days    = d -> d * 1000 * 60 * 60 * 24

// unresolved names will call this, for auto module loading
// TODO maybe not such a good idea, and needs Task.env.path, and caller.file ...
// TODO besides, need to have a dedicated task for this, to prevent races
_env = Env.current
_loaded = Map.new
_module_path = _io_getenv("TL_MODULE_PATH") or "/usr/local/lib/tl"
_resolve: { name ->
    res = _loaded.get(name)
    if res == _loaded: _throw "Undefined: '$name'"
    if res != undefined: return res
    file = "$(_module_path)/$(name.toText).tl"
    _catch: { e =>
        print "error loading: '$file': $e"
        _loaded.set(name, _loaded)
        _throw "Undefined: '$name'"
    }
    //print "trying: '$file' ..."
    res = _parse(_textFromFile(file), file)(_env)
    //print "res: $res"
    _loaded.set(name, res)
}

// run a repl, or every argument as a file
runmain = { as ->
    if as.size == 0: return repl.run

    file = as.get(0)
    //_catch: e -> _throw "Error in: '$file': $e"
    return _parse(_textFromFile(file), file)(_env, as.slice(1))
}
main = !runmain(args)

// this will only return when there are no more tasks to run
io.ioloop

// we are done ... return status of main task (will throw if main task is in error)
return main.value

