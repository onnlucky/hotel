// boot.tl
// 1. initialize into full std lib
// 2. run the requests file(s) or start a repl
// 3. drop into the io runloop, exiting when no tasks are left

while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

// this is here, so the io lib can use it ...
_io_queue = _io_init()
_io = _io_queue.input

// unresolved names will call this, for auto module loading
// TODO maybe not such a good idea, and needs Task.env.path, and caller.file ...
_env = Env.current
_loaded = Map.new
_resolve block=(name ->
    res = _loaded.get(name)
    if res == _loaded: _throw "Undefined: '$name'"
    if res != undefined: return res
    file = "$(name.toText).tl"
    _catch: (e ->
        print "error: '$file': $e"
        _loaded.set(name, _loaded)
        _throw "Undefined: '$name'"
    )
    //Task.env.path.each: (path ->
    print "trying: '$file' ..."
    //if not _isReadableFile(file): continue
    res = _parse(_textFromFile(file), file)(_env)
    _loaded.set(name, res)
)

// run a repl, or every argument as a file
runmain = (as ->
    if as.size == 0: return repl.run

    file = as.get(0)
    _catch: e -> _throw "Unable to run: '$file': $e"
    return _parse(_textFromFile(file), file)(_env)

    // TODO do this instead ...
    as.each: (arg ->
        file = "$arg.tl"
        catch: e -> _throw "Unable to run: '$file': $e"
        _parse(_textFromFile(file), file)(_env)
    )
)
main = !runmain(args)

// the main task will drop into the event loop
loop: (
    Task.yield
    loop: (
        msg = _io_queue.poll
        if msg: print "MSG:", msg.name
        (
            _match(msg == null): break
            _match(msg.name == #waitread): _io_waitread msg.get(0), msg
            _match(msg.name == #waitwrite): _io_waitwrite msg.get(0), msg
            _match(msg.name == #wait): _io_wait msg.get(0), msg
            _nomatch
        )
    )
    if not _io_haswaiting(): break
    _io_run
)

// we are done ... return status of main task (will throw if main task is in error)
return main.value

