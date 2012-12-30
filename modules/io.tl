// io library

_io_queue = _io_init()
_io = _io_queue.input

// the boot task will call this, this is basically hotelvm's mainloop
ioloop = { ->
    loop: {
        Task.yield
        loop: {
            msg = _io_queue.poll
            (
                _match(msg == null): break
                _catch: e -> msg.throw(e.msg)
                _match(msg.name == #waitread): _io_waitread msg.get(0), msg
                _match(msg.name == #waitwrite): _io_waitwrite msg.get(0), msg
                _match(msg.name == #wait): _io_wait msg.get(0), msg
                _match(msg.name == #close): _io_close msg.get(0); msg.reply
                _match(msg.name == #launch): msg.reply(_io_launch(
                        msg.get(0), msg.get(1),
                        msg.get(2), msg.get(3), msg.get(4), msg.get(5),
                        msg.get(6), msg.get(7)
                        ))
                _nomatch
            )
        }
        if not _io_haswaiting(): break
        _io_run
    }
}

// reader/writer utils
readSome = { reader, buf ->
    loop: {
        len = reader.read buf
        if len: return len
        _io.waitread reader
    }
}
readFull = { reader, buf ->
    total = Var.new 0
    loop: {
        len = reader.read buf
        if len == 0: return $total
        if len: $total += len; continue
        _io.waitread reader
    }
}
writeFull = { writer, buf ->
    total = Var.new 0
    loop: {
        if buf.size == 0: return
        len = writer.write buf
        if len == 0: return buf.size
        if not len: _io.waitwrite writer
    }
}

Stream = {
    // TODO optimize the read/write buffer usage
    new: file -> { file: file, class: this.class, rbuf: Buffer.new, wbuf: Buffer.new }
    class: {
        read: { buf ->
            reader = this.file.reader
            if buf: {
                _with_lock(reader, buf): {
                    buf.write this.rbuf
                    len = readSome(reader, buf)
                    if len == 0: return null
                    return buf
                }
            }
            _with_lock(reader, this.rbuf): {
                len = readFull(reader, this.rbuf)
                if len == 0: return null
                return this.rbuf.read
            }
        }
        write: { val ->
            assert val
            // TODO throw exception if not all could be written
            writer = this.file.writer
            if isBuffer(val): {
                _with_lock(writer, val): {
                    assert(this.wbuf.size == 0)
                    return writeFull(writer, val)
                }
            }
            _with_lock(writer, this.wbuf): {
                this.wbuf.write val.toString
                return writeFull(writer, this.wbuf)
            }
        }
        close: { ->
            if this.file.isClosed: return
            _io.close this.file
        }
    }
}

Server = {
    new: file -> { file: file, class: this.class }
    class: {
        accept: { ->
            reader = this.file.reader
            _with_lock(reader): {
                loop: {
                    conn = reader.accept()
                    if conn: return Stream.new(conn)
                    if reader.isClosed: return
                    _io.waitread reader
                }
            }
        }
        close: { ->
            if this.file.isClosed: return
            _io.close this.file
        }
        serve: { ->
            block = args.block; if not block: _throw "serve expects a block"
            loop: {
                client = this.accept
                block(client); client.close
            }
        }
    }
}

File = {
    call: name -> { name: name, class: this.class }
    class: {
        read: { buf ->
            reader = _File_open(this.name, _File_RDONLY).reader
            if buf: {
                _with_lock(reader, buf): {
                    len = readFull(reader, buf)
                    if len == 0: return null
                    return buf
                }
            }
            buf = Buffer.new
            _with_lock(reader, buf): {
                len = readFull(reader, buf)
                if len == 0: return null
                return buf.read
            }
        }
        write: { val ->
            writer = _File_open(this.name, _File_WRONLY+_File_TRUNC+_File_CREAT).writer
            if isBuffer(val): {
                _with_lock(writer, val): {
                    return writeFull(writer, val)
                }
            }
            buf = Buffer.new; buf.write val.toString
            _with_lock(writer, buf): {
                return writeFull(writer, buf)
            }
        }
        open: { ->
            mode = Var.new 0
            if args.map[#truncate]: $mode += _File_TRUNC
            if args.map[#create]: $mode += _File_CREAT
            if not args.map[#readonly]: $mode += _File_RDWR
            if args.map[#readonly]: $mode += _File_RONLY
            Stream.new(_File_open(this.name, $mode))
        }
    }
}

Dir = {
    call: name -> { name: name, class: this.class }
    class: {
        each: { fn ->
            block = (args.block) or fn
            if not block: _throw "each expects a block or fn"
            _Dir_open(this.name).each(block=block)
        }
    }
}

Path = {
    call: name -> { name: name, class: this.class, _stat: _Var_new() }
    class: {
        stat: { ->
            if not _var_get(this._stat): _var_set(this._stat, _Path_stat(this.name))
            _var_get(this._stat)
        }
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

Socket = {
    open: { address, port ->
        ip = _Socket_resolve(address)
        Stream.new(_Socket_connect(ip, port))
    }
    listen: { port ->
        Server.new(_ServerSocket_listen(port))
    }
}

run = { cmd ->
    list = cmd.split
    check = not (try: not args.names.error)
    // no stdin, no stdin, no stderr, joined stdin/stderr
    child = _io.launch(Task.locals.cwd, list, false, true, false, true, args.names?env, false)
    buf = Buffer.new
    out = Stream.new(child.out)
    loop: {
        res = out.read buf
        if not res: break
    }
    r = child.wait
    if r != 0 and check: Error("'$cmd' exit: $r", output=buf.read).throw
    return buf.read, r
}

wait = s -> _io.wait s

stdin = Stream.new(_File_from(0))
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
    wait: wait
    ioloop: ioloop

    stdin: stdin, stdout: stdout, stderr: stderr
    chdir: _io_chdir
    mkdir: _io_mkdir
    getenv: _io_getenv
}

