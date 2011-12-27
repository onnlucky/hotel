// boot.tl
// 1. initialize into full std lib (some of it is in controlflow.c)
// 2. run the requests file(s) or start a repl
// 3. drop into the io runloop, exiting when no tasks are left

// TODO print false or true -> is error because false becomes function call (parser error)
or = (left, &right ->
    if not left: return right()
    return left
)
and = (left, &right ->
    if left: return right()
    return left
)
while = (&cond ->
    block = args.block; if not block: _throw "while expects a block"
    loop: (
        if not cond(): break
        block()
    )
)
try = ( ->
    if not args.block: _throw "try expects a block"
    _catch: return null
    args.block.call()
)

// TODO should have special boot places for text, list and such
_install(Text.class, #split, (split = (on ->
    on = (on) or " " // TODO this is weird in the parser; hence the braces
    onlen = on.size
    loop = (start, at, list->
        if this.startsWith(on, at): (
            goto(loop(at + onlen, at + onlen, list.add(this.slice(start, at))))
        )
        if this.size <= at: (
            return list.add(this.slice(start, at))
        )
        goto(loop(start, at + 1, list))
    )
    return loop(0, 0, [])
)))
_install(List.class, #join, (join = (sep ->
    if this.size == 0: return ""
    sep = (sep) or ""

    var $res = this.get(0).toText
    var $i = 1
    while $i < this.size: (
        $res = $res.cat(sep).cat(this.get($i).toText)
        $i += 1
    )
    return $res
)))

// TODO should not sleep in ms, but need float ... should also add Time module
sleep = (ms -> io.wait ms)
seconds = (s -> s * 1000)
minutes = (m -> m * 1000 * 60)
hours   = (h -> m * 1000 * 60 * 60)
days    = (d -> d * 1000 * 60 * 60 * 24)

// unresolved names will call this, for auto module loading
// TODO maybe not such a good idea, and needs Task.env.path, and caller.file ...
// TODO besides, need to have a dedicated task for this, to prevent races
_env = Env.current
_loaded = Map.new
_module_path = _io_getenv("TL_MODULE_PATH") or "/usr/local/lib/tl"
_resolve block=(name ->
    res = _loaded.get(name)
    if res == _loaded: _throw "Undefined: '$name'"
    if res != undefined: return res
    file = "$(_module_path)/$(name.toText).tl"
    _catch: (e ->
        print "error loading: '$file': $e"
        _loaded.set(name, _loaded)
        _throw "Undefined: '$name'"
    )
    //print "trying: '$file' ..."
    res = _parse(_textFromFile(file), file)(_env)
    //print "res: $res"
    _loaded.set(name, res)
)

// run a repl, or every argument as a file
runmain = (as ->
    if as.size == 0: return repl.run

    file = as.get(0)
    //_catch: e -> _throw "Error in: '$file': $e"
    return _parse(_textFromFile(file), file)(_env, as.slice(1))
)
main = !runmain(args)

// this will only return when there are no more tasks to run
io.ioloop

// we are done ... return status of main task (will throw if main task is in error)
return main.value

