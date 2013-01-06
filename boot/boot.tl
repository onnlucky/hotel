// boot.tl
// 1. initialize into full std lib (some of it is in controlflow.c)
// 2. run the requests file(s) or start a repl
// 3. drop into the io runloop, exiting when no tasks are left

catch: e -> print "boot exception: $(e.toString)"

or = { left, &right ->
    if not left: return right()
    return left
}
and = { left, &right ->
    if left: return right()
    return left
}
while = { &cond ->
    block = args.block; if not block: throw "while expects a block"
    loop: {
        if not cond(): break
        block()
    }
}
min = { l, r ->
    if l <= r: return l
    return r
}
max = { l, r ->
    if l >= r: return l
    return r
}
try = { ->
    if not args.block: throw "try expects a block"
    catch: return null
    args.block.call()
}
_get_random = { ->
    this.get(random(this.size))
}
_each = { fn ->
    block = args.block or fn
    if not block: throw "each expects a block or fn"
    len = this.size
    loop = { at =>
        if at >= len: return
        block(this.get(at), at)
        goto(loop(at + 1))
    }
    loop(0)
}
_find = { fn ->
    block = args.block or fn
    if not block: throw "find expects a block or fn"
    len = this.size
    loop = { at =>
        if at >= len: return null
        e = this.get(at)
        if block(e, at): return e, at
        goto(loop(at + 1))
    }
    loop(0)
}
_filter = { fn ->
    block = args.block or fn
    if not block: throw "filter expects a block or fn"
    arr = Array.new
    len = this.size
    loop = { at =>
        if at >= len: return arr.toList
        e = this.get(at)
        if block(e, at): arr.add(e)
        goto(loop(at + 1))
    }
    loop(0)
}
_times = { fn ->
    if this <= 0: return
    block = args.block or fn
    if not block: throw "times expects a block or fn"
    again = { at ->
        block(at)
        if at >= this: return
        goto again(at + 1)
    }; goto again(1)
}
_to = { to, fn ->
    block = args.block or fn
    if not block: throw "to expects a block or fn"
    to = to or 1
    if (this <= to): {
        again = { at ->
            block(at)
            if at >= to: return
            goto again(at + 1)
        }; goto again(this)
    }
    again = { at ->
            block(at)
            if at <= to: return
            goto again(at - 1)
    }; goto again(this)
}

_install(Int.class, #times, _times)
_install(Int.class, #to, _to)

// TODO should have special boot places for string, list and such
/// String.each: for ever character run block or function given
_install(String.class, #each, _each)
/// String.split: split the string wherever args[1] occurs, returns a list of string fragments
_install(String.class, #split, (split = { on ->
    if this.size == 0: return []
    on = on or " "
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
/// String.replace: replace the first occurance of args[1] with args[2], returns a new #String
_install(String.class, #replace, (replace = { from, to ->
    at = this.find(from)
    if not at: return this

    before = this.slice(0, at)
    after = this.slice(at + from.size)
    return "$before$to$after"
}))

_install(Set.class, #random, _get_random)
_install(Set.class, #each, _each)
_install(List.class, #random, _get_random)
_install(List.class, #each, _each)
_install(List.class, #find, _find)
_install(List.class, #filter, _filter)
_install(List.class, #join, (join = { sep ->
    if this.size == 0: return ""
    sep = sep or ""

    var $res = this.get(0).toString
    var $i = 1
    while $i < this.size: {
        $res = $res.cat(sep).cat(this.get($i).toString)
        $i += 1
    }
    return $res
}))
_install(List.class, #reduce, (reduce = { fn ->
    block = args.block or fn
    if not block: throw "reduce expects a block or fn"
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
    block = args.block or fn
    if not block: throw "map expects a block or fn"
    if this.size == 0: return this

    res = _List_unsafe(this.size)

    var $i = 0
    while $i < this.size: {
        _list_set_(res, $i, block(this.get($i)))
        $i += 1
    }
    return res
}))
_flatten2 = { ls, arr ->
    ls.each: { e ->
        if isList(e): _flatten2(e, arr); return
        arr.add(e)
    }
}
_install(List.class, #flatten, (flatten = { ->
    arr = Array.new
    _flatten2(this, arr)
    return arr.toList
}))
_install(Array.class, #random, _get_random)
_install(Array.class, #each, _each)
_install(Array.class, #find, _find)
_install(Array.class, #join, (join = { sep ->
    if this.size == 0: return ""
    sep = sep or ""

    var $res = this.get(0).toString
    var $i = 1
    while $i < this.size: {
        $res = $res.cat(sep).cat(this.get($i).toString)
        $i += 1
    }
    return $res
}))
_errorConstructor = { msg ->
    stack = Task.stacktrace(1)
    args.map.set(#stack, stack).set(#msg, msg).set(#class, this.class).toObject
}
_install(Error, #call, _errorConstructor);
_install(ArgumentError, #call, _errorConstructor);
_install(UndefinedError, #call, _errorConstructor);
_install(Error, #class, {
    throw: { -> throw this }
    toString: { ->
        ls = Array.new
        ls.add "Error: ", this.msg, "\n"
        x = Var.new 0
        loop: {
            file, fn, line = this.stack.get($x)
            if not file: break
            ls.add "  at ", file, ":", line, " ", fn, "\n"
            $x += 1
        }
        return ls.join
    }
})

inspect = { x ->
    toJSON = { x, indent, buf, inlist -> (
        _match(isBool(x)): buf.write(x.toString); return false
        _match(not x): buf.write("null"); return false
        _match(isNumber(x)): buf.write(x.toString); return false
        _match(isSym(x)): buf.write("#"); buf.write(x.toString.escape); return false
        _match(isString(x)): buf.write("\""); buf.write(x.escape); buf.write("\""); return false
        _match(isList(x)): (
            if x.size == 0: buf.write("[]"); return false
            if indent.size > 0: buf.write("\n")
            x.each: { v ->
                buf.write(indent); buf.write("- ")
                nl = toJSON(v, indent.cat(" "), buf, true)
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
                buf.write(key.toString); buf.write(": ")
                nl = toJSON(_Map_get(x, key), indent.cat(" "), buf)
                if not nl: buf.write("\n")
                $first = false
            }
            return true
        )
        _match(true): (
            buf.write("$x"); return false
        )
        _nomatch()
    )}
    buf = io.Buffer.new
    toJSON(x, "", buf)
    buf.read
}

// TODO should report line number and such of testcase ...
test = { ->
    catch: e => print "TEST FAILED:", args.slice.join(" "); print e.toString
    args.block.call
}

// TODO should not sleep in ms, but need float ... should also add Time module
sleep =   s -> io.wait s
wait  =   s -> io.wait s
millis  = s -> s / 1000.0
seconds = s -> s
minutes = m -> m * 60
hours   = h -> m * 60 * 60
days    = d -> d * 60 * 60 * 24

eval2 = { string -> _parse(string)(Env.current) }
/// String.eval: evalute the string as a script to run in the current environment
_install(String.class, #eval, (_eval_ = { ->
    eval2(this)
}))

// unresolved names will call this, for auto module loading
// TODO maybe not such a good idea, and needs Task.env.path, and caller.file ...
// TODO besides, need to have a dedicated task for this, to prevent races
_env = Env.current
_loaded = HashMap.new
_module_path = _io_getenv("TL_MODULE_PATH") or "/usr/local/lib/tl"
_fileForModule = { name, path ->
    file = "$path/$name"
    if _Path_stat(file).ino != 0: return file
    file = "$(_module_path)/$name"
    if _Path_stat(file).ino != 0: return file
    Error("Undefined: '$name'").throw
}
_resolve: { name, path ->
    res = _loaded.get(name)
    if res == _loaded: throw "Undefined: '$name'"
    if isDefined res: return res
    catch: e -> throw "Error reading module: '$name', $(e?msg)"
    file = _fileForModule("$(name.toString).tl", path)
    res = _parse(_stringFromFile(file), file)(_env)
    _loaded.set(name, res)
}

// run a repl, or every argument as a file
runmain = { as ->
    if as.size == 0: return repl.run

    catch: { e ->
        print e.toString
        _set_exitcode(1)
    }
    file = as.get(0)
    return _parse(_stringFromFile(file), file)(_env, as.slice(1))
}
main = !runmain(args)

// this will only return when there are no more tasks to run
io.ioloop

// we are done, print the results, if any, error has already been printed
if main.value: print main.value
return main.value

