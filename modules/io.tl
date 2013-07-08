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

/// A Stream is a lot like a buffer, but we read and write from some source
Stream = {
    // TODO optimize the read/write buffer usage
    new: file -> { file: file, class: this.class, rbuf: Buffer.new, wbuf: Buffer.new }
    class: {
        _readSome: ->
            reader = this.file.reader
            _with_lock(reader, this.rbuf):
                len = readSome(reader, this.rbuf)
        _readFull: ->
            reader = this.file.reader
            _with_lock(reader, this.rbuf): readFull(reader, this.rbuf)
        _writeOne: val ->
            if not val: return
            writer = this.file.writer
            // TODO throw exception if not all could be written
            if isBuffer(val):
                _with_lock(writer, val):
                    return writeFull(writer, val)
            _with_lock(writer, this.wbuf):
                this.wbuf.write val
                return writeFull(writer, this.wbuf)

        readClosed: ->
            return this.rbuf.size == 0 and this.file.reader.isClosed
        size: ->
            return this.rbuf.size
        readByte: ->
            if this.rbuf.size == 0: this._readSome
            return this.rbuf.readByte
        readString: len ->
            return this.read len
        read: len ->
            if len:
                while this.rbuf.size < len: this._readSome
                return this.rbuf.read len
            this._readFull
            return this.rbuf.read
        find: what, from, upto ->
            loop:
                r = this.rbuf.find what, from, upto
                if r: return r
                if upto and this.rbuf.size >= upto: return null
                if this.file.reader.isClosed: return null
                this._readSome

        /// write many of Buffer, String, Binary, or byte (any number); skipping nulls
        write: ->
            args.each: v -> this._writeOne v
        writeclose: ->
            if this.file.isClosed: return
            writer = this.file.writer
            _with_lock(writer): writer.close
        close: ->
            if this.file.isClosed: return
            _io.close this.file
    }
}

Server = {
    new: file -> { file: file, class: this.class }
    class: {
        port: -> this.file.port
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
    output = Stream.new(child.out).read
    r = child.wait
    if r != 0 and check: Error("'$(arglist[1])' exit: $r", output=output).throw
    return output, r

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

