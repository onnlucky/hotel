// sketch of new infrastructure ...

_io_queue = _io_init()
io = _io_queue.input

!(
    sock = _Socket_connect("127.0.0.1", 80)
    buf = _Buffer_new()
    buf.write "GET / HTTP/1.0\r\n\r\n"
    loop: (
        if buf.canread == 0: break
        len = sock.write buf
        if len == null: io.waitwrite sock
    )
    loop: (
        len = sock.read buf
        if len == 0: break
        if len == null: io.waitread sock
    )
    print buf.read
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

