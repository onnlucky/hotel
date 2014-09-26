// hotel http library

HttpConnection = conn ->
    _Mutable_new {
        _req = null, _didhead = false, _didend = false
        _in = io.Buffer.new
        _body = io.Buffer.new
        status = 200
        contentType = "text/plain"
        setContentType = (type ->
            if this._didhead: throw "Already send headers"
            this.contentType = type
        )
        req = (->
            // read and parse until headers are complete
            if this._req: return this._req
            loop:
                if this._in.size > 2048: print this._in.readString; conn.close; throw "REQUEST TOO LONG"
                if this._in.find("\r\n\r\n"): break
                if this._in.find("\n\n"): break
                len = conn.readInto this._in
                if len == 0: throw "IO ERROR"
            this._req = httpparser.request(this._in.readString)
        )
        end = -> this._didend = true; this.flush(true); conn.close
        error = (->
            this.status = 500
            this._didend = true
            this._body.clear; this._body.write("500 internal error")
            this.flush(true); conn.close
        )
        write = (text ->
            if this._didend: throw "Already Ended Response"
            this._body.write text
        )
        flush = (ending ->
            if not this._didhead: this._sendheaders(ending)
            while this._body.size > 0:
                len = conn.write this._body
                //if len == 0: throw "IO ERROR"
        )
        _sendheaders = (ending ->
            this._didhead = true
            headers = io.Buffer.new()
            headers.write "HTTP/1.0 $(this.status) Ok\r\n"
            if this.contentType: headers.write "Content-Type: $(this.contentType)\r\n"
            if ending: headers.write "Content-Length: $(this._body.size)\r\n"
            headers.write "\r\n"
            while headers.size > 0:
                len = conn.write headers
                //if len == 0: throw "IO ERROR"
        )
    }

Server = {
    new = port -> { sock = io.Socket.listen(port), class = this.class }
    class = {
        port = -> this.sock.port
        close = -> this.sock.close
        serve = ( ->
            block = args.block; if not block: throw "expect a block"
            loop:
                client = this.sock.accept
                if not client: return
                conn = HttpConnection(client)
                !(
                    catch: e => print e, e?msg; conn.error
                    block(conn); conn.end
                )
                Task.yield
        )
    }
}

SizedStream = stream, size ->
    var $toread = size
    {
        read = len ->
            if $toread == 0: return null
            if not len: return stream.read $toread
            len = min($toread, len)
            $toread -= len
            return stream.read len
        readString = len ->
            if $toread == 0: return null
            if not len: return stream.readString $toread
            len = min($toread, len)
            $toread -= len
            return stream.readString len
        readByte = -> this.read(1)[1]
        close = ->
            this.read
            return null
    }

readchunklen = stream, len ->
    b = stream.readByte
    { b == '\r'             }: assert stream.read(1) == "\n"; return len
    { b >= '0' and b <= '9' }: return readchunklen(stream, len * 16 + b - '0')
    { b >= 'A' and b <= 'F' }: return readchunklen(stream, len * 16 + b - 'A' + 10)
    { b >= 'a' and b <= 'f' }: return readchunklen(stream, len * 16 + b - 'a' + 10)

readchunk = stream, into ->
    len = readchunklen(stream, 0)
    if len == 0: return null
    res = into.write stream.read len
    assert stream.read(2) == "\r\n"
    return res

readsomechunks = stream, into, min ->
    while into.size < min:
        res = readchunk stream, into
        if not res: return

ChuckedStream = stream ->
    buf = Buffer.new
    {
        read = len ->
            readsomechunks(stream, buf, len)
            buf.read len
        readString = len ->
            readsomechunks(stream, buf, len)
            buf.readString len
        readByte = -> this.read(1)[1]
        close = -> stream.close
    }

InflateStream = stream, gzip ->
    {
        read = ->
            zlib.inflate(stream.read, gzip)
        readString = ->
            this.read.toString
    }

openstream = url ->
    if url.type == "https":
        return ssl.Stream.new(io.Socket.open(url.host, url.port or 443))
    return io.Socket.open(url.host, url.port or 80)

get = url ->
    url = httpparser.url(url)
    conn = openstream(url)
    conn.write "GET "+ url.request +" HTTP/1.0\r\n"
    conn.write "Host: "+ url.host +"\r\n"
    conn.write "User-Agent: Mozilla/5.0\r\n"
    conn.write "\r\n"

    // TODO now we can just parse without "finding" first ...
    at = conn.find("\r\n\r\n", 0, 8000)
    if not at:
        if conn.size >= 8000: Error("Headers too long").throw
        Error("No headers received").throw

    // TODO take back the stream after body is done ...
    header = conn.readString(at + 4 - 1)
    res = httpparser.response(header)
    length = res.headers["Content-Length"]
    transfer = res.headers["Transfer-Encoding"]
    encoding = res.headers["Content-Encoding"]
    type = res.headers["Content-Type"]
    body = { length                }: SizedStream(conn, length.eval)
           { transfer == "chunked" }: ChuckedStream(conn)
           { transfer              }: assert false, "unknown transfer encoding", transfer
           {                       }: conn
    body = { encoding == "gzip"    }: InflateStream(body, true)
           { encoding == "deflate" }: InflateStream(body)
           { encoding              }: assert false, "unknown encoding", encoding
           {                       }: body

    // TODO read text encoding and not assume utf8
    if args.namesmap["stream"]: return body, res
    if type:
        { type.startsWith("application/json") }: return json.parse(body.readString), res
        { type.startsWith("application/") and type.find("+json") }: return json.parse(body.readString), res
        { type.startsWith("text/json") }: return json.parse(body.readString), res
        { type.startsWith("text/") and type.find("+json") }: return json.parse(body.readString), res
        { type.startsWith("text/") }: return body.readString, res
        { type.startsWith("application/javascript") }: return body.readString, res
        { type.startsWith("application/ecmascript") }: return body.readString, res
        { type.startsWith("application/") and type.find("xml") }: return body.readString, res
        { }: null
    return body.read, res

{{ Server, get }}

