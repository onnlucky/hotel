// sketch of new infrastructure ...
!(
    sock = _Socket_open(_Socket_resolve("google.com"))
    buf = _Buffer_new
    buf.write "GET / HTTP/1.0\r\n\r\n"
    loop: (
        if buf.isEmpty: break
        len = sock.write buf
        if len == #wait: _io_request.write sock
    )
    loop: (
        len = sock.read buf
        if len == 0: break
        if len == #wait: _io_request.read sock
    )
    print buf.read
)

// main task will drop into loop
loop: (
    assert Task.isThreadBound
    Task.yield
    loop: (
        msg = _io_requests.poll
       (_match(msg == null): break
        _match(msg.name == #read): _io_waitread msg.get(0), msg
        _match(msg.name == #write): _io_waitwrite msg.get(0), msg
        _match(msg.name == #wait): _io_wait msg.get(0), msg
        _nomatch)
    )
    //if not System.threading and System.runnable > 1: continue
    _io_run
)

