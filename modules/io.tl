// io library

_io_queue = _io_init()
_io = _io_queue.input

// the boot task will call this
ioloop = { ->
    loop: {
        Task.yield
        loop: {
            msg = _io_queue.poll
            (
                _match(msg == null): break
                _match(msg.name == #waitread): _io_waitread msg.get(0), msg
                _match(msg.name == #waitwrite): _io_waitwrite msg.get(0), msg
                _match(msg.name == #wait): _io_wait msg.get(0), msg
                _match(msg.name == #launch): msg.reply(_io_launch(
                        msg.get(0), msg.get(1), msg.get(2), msg.get(3), msg.get(4)))
                _nomatch
            )
        }
        if not _io_haswaiting(): break
        _io_run
    }
}

Buffer = {
    new: -> { buf: _Buffer_new(), class: this.class }
    toBuffer: { v ->
        // TODO this makes the class sortof "final"
        try: if v.class == this.class: return v
        buf = _Buffer_new()
        buf.write v.toText
        return { buf: buf, class: this.class }
    }
    class: {
        size: -> this.buf.size
        find: v -> this.buf.find v.toText
        canread: -> this.buf.canread
        canwrite: -> this.buf.canwrite
        write: { v ->
            try: if v.class == this.class: return this.buf.writebuf v
            this.buf.write v.toText
        }
        read: { v ->
            try: if v.class == this.class: return v.write this
            return this.buf.read
        }
        toText: -> return this.buf.read
    }
}

readSome = { file, buf ->
    if not buf.class == Buffer.class: _throw "Expected a Buffer"
    reader = file.reader
    _with_lock(buf.buf, reader): {
        loop: {
            len = reader.read buf.buf
            if len == null: _io.waitread reader
            if len: return buf, len
        }
    }
}
readFull = { file, buf ->
    if not buf.class == Buffer.class: _throw "Expected a Buffer"
    reader = file.reader
    _with_lock(buf.buf, reader): {
        loop: {
            len = reader.read buf.buf
            if len == 0: break
            if len == null: _io.waitread reader
        }
    }
    // TODO should be able to return just the buffer, if that behaves a lot like Texts
    return buf.read
}
writeFull = { file, val ->
    writer = file.writer
    buf = Buffer.toBuffer(val)
    _with_lock(buf.buf, writer): {
        loop: {
            if buf.canread == 0: break
            len = writer.write buf.buf
            if len == null: _io.waitwrite writer
        }
    }
}

Stream = {
    new: file -> { file: file, class: this.class }
    class: {
        read: buf -> readSome(this.file, buf or Buffer.new)
        write: val -> writeFull(this.file, Buffer.toBuffer(val))
        close: { ->
            if this.file.isClosed: return
            _with_lock(this.file.reader, this.file.writer): this.file.close
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
                    _io.waitread reader
                }
            }
        }
        close: -> this.file.cose
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
        read: buf -> readFull(_File_open(this.name, _File_RDONLY), buf or Buffer.new)
        write: val -> writeFull(_File_open(this.name, _File_WRONLY+_File_TRUNC+_File_CREAT), Buffer.toBuffer(val))
        open: -> Stream.new(_File_open(this.name, _File_RDRW+_File_TRUNC+File_CREAT))
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

Process = {
    run: { cmd ->
        list = cmd.split
        check = try: not args.names.error
        // no stdin, no stdin, no stderr, joined stdin/stderr
        child = _io.launch list, false, true, false, true
        buf = Buffer.new
        out = Stream.new(child.out)
        loop: {
            _, len = out.read buf
            if len == 0: break
        }
        r = child.wait
        if r != 0 and check: _throw "Error: process: $r $(list.get(0)) $(list.size)"
        return buf.read, r
    }
}

wait = ms -> _io.wait ms

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
    Process: Process
    wait: wait
    ioloop: ioloop

    stdin: stdin, stdout: stdout, stderr: stderr
    chdir: _io_chdir
}

