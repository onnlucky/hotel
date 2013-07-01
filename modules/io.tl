// io library

_io_queue = _io_init()
_io = _io_queue.input

// the boot task will call this, this is basically hotelvm's mainloop
ioloop = ->
    loop:
        Task.yield
        loop:
            msg = _io_queue.poll
            (
                _match(msg == null): break
                catch: e -> msg.throw(e.msg)
                _match(msg.name == #waitread): _io_waitread msg[1], msg
                _match(msg.name == #waitwrite): _io_waitwrite msg[1], msg
                _match(msg.name == #wait): _io_wait msg[1], msg
                _match(msg.name == #close): _io_close msg[1]; msg.reply
                _match(msg.name == #launch): msg.reply(_io_launch(
                        msg[1], msg[2],
                        msg[3], msg[4], msg[5], msg[6],
                        msg[7], msg[8]
                        ))
                _nomatch
            )
        if not _io_haswaiting(): break
        _io_run

// reader/writer utils
readSome = reader, buf ->
    loop:
        len = reader.read buf
        if len: return len
        _io.waitread reader

readFull = reader, buf ->
    total = Var.new 0
    loop:
        len = reader.read buf
        if len == 0: return $total
        if len: $total += len; continue
        _io.waitread reader

writeFull = writer, buf ->
    total = Var.new 0
    loop:
        if buf.size == 0: return
        len = writer.write buf
        if len == 0: return buf.size
        if not len: _io.waitwrite writer

Stream = {
    // TODO optimize the read/write buffer usage
    new: file -> { file: file, class: this.class, rbuf: Buffer.new, wbuf: Buffer.new }
    class: {

        read: (buf ->
            reader = this.file.reader
            if buf:
                _with_lock(reader, buf):
                    buf.write this.rbuf
                    len = readSome(reader, buf)
                    if len == 0: return null
                    return buf
            _with_lock(reader, this.rbuf):
                len = readFull(reader, this.rbuf)
                if len == 0: return null
                return this.rbuf.read
        )

        write: (val ->
            assert val
            // TODO throw exception if not all could be written
            writer = this.file.writer
            if isBuffer(val):
                _with_lock(writer, val):
                    assert(this.wbuf.size == 0)
                    return writeFull(writer, val)
            _with_lock(writer, this.wbuf):
                this.wbuf.write val.toString
                return writeFull(writer, this.wbuf)
        )
        writeclose: (->
            if this.file.isClosed: return
            writer = this.file.writer
            _with_lock(writer): writer.close
        )
        close: (->
            if this.file.isClosed: return
            _io.close this.file
        )
    }
}

Server = {
    new: file -> { file: file, class: this.class }
    class: {
        accept: (->
            reader = this.file.reader
            _with_lock(reader):
                loop:
                    conn = reader.accept()
                    if conn: return Stream.new(conn)
                    if reader.isClosed: return
                    _io.waitread reader
        )
        close: (->
            if this.file.isClosed: return
            _io.close this.file
        )

        serve: (->
            block = args.block; if not block: throw "serve expects a block"
            loop:
                client = this.accept
                block(client); client.close
        )
    }
}

File = {
    call: name -> { name: name, class: this.class }
    class: {
        read: (buf ->
            reader = _File_open(this.name, _File_RDONLY).reader
            if buf:
                _with_lock(reader, buf):
                    len = readFull(reader, buf)
                    if len == 0: return null
                    return buf
            buf = Buffer.new
            _with_lock(reader, buf):
                len = readFull(reader, buf)
                if len == 0: return null
                return buf.read
        )

        write: (val ->
            writer = _File_open(this.name, _File_WRONLY+_File_TRUNC+_File_CREAT).writer
            if isBuffer(val):
                _with_lock(writer, val):
                    return writeFull(writer, val)
            buf = Buffer.new; buf.write val.toString
            _with_lock(writer, buf):
                return writeFull(writer, buf)
        )

        open: (->
            mode = Var.new 0
            if args.map[#truncate]: $mode += _File_TRUNC
            if args.map[#create]: $mode += _File_CREAT
            if not args.map[#readonly]: $mode += _File_RDWR
            if args.map[#readonly]: $mode += _File_RONLY
            Stream.new(_File_open(this.name, $mode))
        )
    }
}

Dir = {
    call: name -> { name: name, class: this.class }
    class: {
        each: (fn ->
            block = (args.block) or fn
            if not block: throw "each expects a block or fn"
            _Dir_open(this.name).each(block=block)
        )
    }
}

Path = {
    call: name -> { name: name, class: this.class, _stat: _Var_new() }
    class: {
        stat: (->
            if not _var_get(this._stat): _var_set(this._stat, _Path_stat(this.name))
            _var_get(this._stat)
        )
        exists: -> this.stat.ino != 0
        isFile: -> binand(this.stat.mode, _Stat_IFMT) == _Stat_IFREG
        isDir:  -> binand(this.stat.mode, _Stat_IFMT) == _Stat_IFDIR
        isLink: -> binand(this.stat.mode, _Stat_IFMT) == _Stat_IFLNK
        asFile: -> File(this.name)
        asDir:  -> Dir(this.name)
        create: -> _io_mkdir(this.name)
        delete: -> _io_rmdir(this.name)
        unlink: -> _io_unlink(this.name)
    }
}

UdpSocket = {
    new: file -> { file: file, class: this.class }
    class: {
        sendto: ip, port, buf -> _Socket_sendto(this.file, ip, port, buf)
        recvfrom: (buf ->
            reader = this.file.reader
            loop:
                _with_lock(reader, buf):
                    buf, ip, port = _Socket_recvfrom(this.file, buf)
                    if buf: return buf, ip, port
                    _io.waitread reader
        )
    }
}

Socket = {
    open: (address, port ->
        ip = _Socket_resolve(address)
        Stream.new(_Socket_connect(ip, port))
    )
    listen: (port ->
        Server.new(_ServerSocket_listen(port))
    )
    udp: (port, broadcast ->
        UdpSocket.new(_Socket_udp(port, broadcast))
    )
}

_run = arglist, env, check ->
    // no stdin, no stdin, no stderr, joined stdin/stderr
    child = _io.launch(Task.locals.cwd, arglist, false, true, false, true, env, false)
    buf = Buffer.new
    out = Stream.new(child.out)
    loop:
        res = out.read buf
        if not res: break
    r = child.wait
    if r != 0 and check: Error("'$(arglist[1])' exit: $r", output=buf.read).throw
    return buf.read, r

/// run(cmdline, args...) env=null, allowerror=false
/// execute cmdline.split.first as found in PATH environment variable, passing cmdline.split[1:].cat(args) as arguments
/// will throw an error when command returns nonzero status
/// will return output of stdin and stderr combined
run = cmd ->
    arglist = cmd.toString.split.cat(args[2:].map(a -> a.toString))
    _run(arglist, args.names.env, not args.names.allowerror)

/// shell(cmd, args...) env=null, allowerror=false
/// execute cmd in a unix compatible shell, extra arguments will become positional arguments $0, $1 ...
/// will throw an error when command returns nonzero status
/// will return output of stdin and stderr combined
shell = cmd ->
    arglist = ["sh", "-c", cmd.toString].cat(args[2:].map(a -> a.toString))
    _run(arglist, args.names.env, not args.names.allowerror)

wait = s -> _io.wait s

Terminal = {
    new: file -> { file: file, class: this.class, rbuf: Buffer.new, wbuf: Buffer.new }
    class: {
        class: Stream.class

        isTTY: -> _tty_is(this.file)
        setRaw: -> _tty_setRaw(this.file)
        restore: -> _tty_restore(this.file)
        size: -> _tty_size(this.file)
    }
}

stdin = Terminal.new(_File_from(0))
stdout = Stream.new(_File_from(1))
stderr = Stream.new(_File_from(2))

return {
    Buffer: Buffer
    Stream: Stream
    File: File
    Dir: Dir
    Path: Path
    Socket: Socket

    run: run
    shell: shell

    wait: wait
    ioloop: ioloop

    stdin: stdin, stdout: stdout, stderr: stderr
    chdir: _io_chdir
    mkdir: _io_mkdir
    getenv: _io_getenv
}

