// a stream interface writing through openssl, onto a lower level stream, like a socket

Stream = {
    new= stream -> { stream= stream, ssl= openssl.newCodec, class= this.class }
    class= {
        read= (into ->
            if into: return this._read2(into)
            buf = Buffer.new
            loop:
                res = this._read2(buf)
                if not res: break
            if buf.size > 0: return buf.read
            return null
        )
        _read2= (into ->
            ssl = this.ssl
            _with_lock(ssl, into):
                loop:
                    len = ssl.read(into)
                    if ssl.wbuf.size > 0: this.stream.write ssl.wbuf
                    if len > 0: return into
                    res = this.stream.read ssl.rbuf
                    if not res: return null
        )
        write= (from ->
            if isBuffer(from): return this._write2(from)
            buf = Buffer.new; buf.write(from.toString)
            return this._write2(buf)
        )
        _write2= (from ->
            ssl = this.ssl
            _with_lock(ssl, from):
                loop:
                    mustread = ssl.write(from)
                    this.stream.write ssl.wbuf
                    if mustread:
                        res = this.stream.read ssl.rbuf
                        if not res: return 1 // TODO figure out how much was written ...
                        continue
                    if from.size == 0 and ssl.wbuf.size == 0: return
        )
    }
}

{{ Stream }}

