// sketch of new infrastructure ...

map = Map.new
print map.get(#foobar)
print map.set(#foobar, 42)
print map.get(#foobar)

/*
env = Env.current
_resolve: (name ->
    _catch: e -> print "loader: $e"
    filename = name.toText.cat(".tl")
    file = _textFromFile(filename)
    code = _parse(file, filename)
    code(env)
)
return helloworld

/*
load = (name ->
    _parse(_textFromFile(name), name)(Env.current)
)
io = load("helloworld.tl")
print io

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

