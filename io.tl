// io library

Buffer = {
    new: -> { buf: _Buffer_new(), class: this.class }
    toBuffer: (v ->
        // TODO this makes the class sortof "final"
        try: if v.class == this.class: return v
        buf = _Buffer_new()
        buf.write v.toText
        return { buf: buf, class: this.class }
    )
    class: {
        size: -> this.buf.size
        canread: -> this.buf.canread
        canwrite: -> this.buf.canwrite
        write: (v ->
            try: if v.class == this.class: return this.buf.writebuf v
            this.buf.write v.toText
        )
        read: (v ->
            try: if v.class == this.class: return v.write this
            return this.buf.read
        )
        toText: -> return this.buf.read
    }
}

readSome = (file, buf ->
    if not buf.class == Buffer.class: _throw "Expected a Buffer"
    reader = file.reader
    _with_lock(buf.buf, reader): (
        loop: (
            len = reader.read buf.buf
            print "readSome: len: $len"
            if len == null: _io.waitread reader
            if len: break
        )
    )
    return buf
)
readFull = (file, buf ->
    if not buf.class == Buffer.class: _throw "Expected a Buffer"
    reader = file.reader
    _with_lock(buf.buf, reader): (
        loop: (
            len = reader.read buf.buf
            if len == 0: break
            if len == null: _io.waitread reader
        )
    )
    // TODO should be able to return just the buffer, if that behaves a lot like Texts
    return buf.read
)
writeFull = (file, val ->
    writer = file.writer
    buf = Buffer.toBuffer(val)
    _with_lock(buf.buf, writer): (
        loop: (
            if buf.canread == 0: break
            len = writer.write buf.buf
            if len == null: _io.waitwrite writer
        )
    )
)

Stream = {
    new: file -> { file: file, class: this.class }
    class: {
        read: (buf -> readSome(this.file, buf or Buffer.new))
        write: (val -> writeFull(this.file, Buffer.toBuffer(val)))
        close: -> this.file.close
    }
}

Server = {
    new: file -> { file: file, class: this.class }
    class: {
        accept: (->
            reader = this.file.reader
            _with_lock(reader): (
                loop: (
                    conn = reader.accept()
                    if conn: return Stream.new(conn)
                    _io.waitread reader
                )
            )
        )
        close: -> this.file.cose
        serve: (->
            block = args.block; if not block: _throw "serve expects a block"
            loop: (
                client = this.accept
                block(client) //; client.close
            )
        )
    }
}

File = {
    call: (name -> { name: name, class: this.class })
    class: {
        read: (buf -> readFull(_File_open(this.name, _File_RDONLY), buf or Buffer.new))
        write: val -> writeFull(_File_open(this.name, _File_WRONLY+_File_TRUNC+_File_CREAT), Buffer.toBuffer(val))
        open: -> Stream.new(_File_open(this.name, _File_RDRW+_File_TRUNC+File_CREAT))
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
}

return { Buffer: Buffer, Stream: Stream, File: File, Socket: Socket }

