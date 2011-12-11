// io library

readFull = (file ->
    reader = file.reader
    buf = _Buffer_new()
    _with_lock(buf, reader): (
        loop: (
            len = reader.read buf
            if len == 0: break
            if len == null: _io.waitread reader
        )
    )
    // TODO should be able to return just the buffer, if that behaves a lot like Texts
    return buf.read
)
writeFull = (file, value ->
    writer = file.writer
    buf = _Buffer_new()
    buf.write value
    _with_lock(buf, writer): (
        loop: (
            if buf.canread == 0: break
            len = writer.write buf
            if len == null: _io.waitwrite writer
        )
    )
)

File = {
    call: (name -> { name: name, class: this.class })
    class: {
        read: -> readFull(_File_open(this.name, _File_RDONLY))
        write: v -> writeFull(_File_open(this.name, _File_WRONLY+_File_TRUNC+_File_CREAT), v)
    }
}

return { File: File }

/*
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

test = (->
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

