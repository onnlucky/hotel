// sketch of new infrastructure ...

_each_index = (->
    if not args.block: _throw "each expects a block"
    var $at = -1; size = this.size; block = args.block
    loop: (
        $at += 1
        if $at >= size: break
        block(this.get($at))
    )
    block(42)
)
_each_index(this=[1, 2, 3]): (n->print n)

_env = Env.current
_loaded = Map.new
_resolve block=(name ->
    res = _loaded.get(name)
    if res == _loaded: _throw "Undefined: '$name'"
    if res != undefined: return res
    _catch: (e ->
        _loaded.set(name, _loaded)
        _throw "Undefined: '$name'"
    )
    //Task.env.path.each: (path ->
    file = "$(name.toText).tl"
    //if not _isReadableFile(file): continue
    res = _parse(_textFromFile(file), file)(_env)
    _loaded.set(name, res)
)

print "here"
print test
print test.foobar
print test.foobar
print test.foobar
print "DONE"
return 42

/*
io = load("boot.tl")
io.run


_io_queue = _io_init()
io = _io_queue.input

// how a real socket could look ... need some more ... ahum modules ...
Socket = {
    write: (v ->
        buf = Buffer.toBuffer(v)
        reader = @reader
        with(buf, reader): (
            loop = (written ->
                if buf.canread == 0: break
                len = writer.write buf
                if len == null: throw "Error: Closed"
                if len == 0: io.waitwrite writer
                goto loop(written + len)
            )
            return loop(0)
        )
    )
}

!(
    sock = _Socket_connect("127.0.0.1", 80)
    buf = _Buffer_new()
    reader = sock.reader
    writer = sock.writer
    with(buf, reader, writer): (
        buf.write "GET / HTTP/1.0\r\n\r\n"
        loop: (
            if buf.canread == 0: break
            len = writer.write buf
            if len == null: io.waitwrite writer
        )
        loop: (
            len = reader.read buf
            if len: break
            if len == null: io.waitread reader
        )
        print buf.read
    )
)

!(io.wait 2000; print "DONE")

// main task will drop into loop
loop: (
    Task.yield
    loop: (
        msg = _io_queue.poll
        if msg: print "MSG:", msg.name
       (_match(msg == null): break
        _match(msg.name == #waitread): _io_waitread msg.get(0), msg
        _match(msg.name == #waitwrite): _io_waitwrite msg.get(0), msg
        _match(msg.name == #wait): _io_wait msg.get(0), msg
        _nomatch)
    )
    if not _io_haswaiting(): break
    _io_run
)

