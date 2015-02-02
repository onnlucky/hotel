// hotel http library; this is a bit of a toy library

MAX_HEADERS = 50 * 1024 + 1

SizedStream = stream, size ->
    var $toread = size
    {
        read = len ->
            if $toread == 0: return null
            if not len: return stream.read($toread)
            len = min($toread, len)
            $toread -= len
            return stream.read(len)
        readString = len ->
            if $toread == 0: return null
            if not len: return stream.readString($toread)
            len = min($toread, len)
            $toread -= len
            return stream.readString(len)
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
    res = into.write(stream.read(len))
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
            buf.read(len)
        readString = len ->
            readsomechunks(stream, buf, len)
            buf.readString(len)
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

// TODO read text encoding and not assume utf8
// TODO allow more control over decoding of type
bodyFromConnection = conn, headers, stream, raw ->
    length = headers["Content-Length"]
    transfer = headers["Transfer-Encoding"]
    encoding = headers["Content-Encoding"]
    type = headers["Content-Type"]
    body = { length                }: SizedStream(conn, length.eval)
           { transfer == "chunked" }: ChuckedStream(conn)
           { transfer              }: assert false, "unknown transfer encoding", transfer
           { raw                   }: conn
           {                       }: Buffer.new
    body = { encoding == "gzip"    }: InflateStream(body, true)
           { encoding == "deflate" }: InflateStream(body)
           { encoding              }: assert false, "unknown encoding", encoding
           {                       }: body

    if stream: return body
    if type:
        { type.startsWith("application/x-www-form-urlencoded") }: return httpparser.formdata(body.readString)
        { type.startsWith("application/json") }: return json.parse(body.readString)
        { type.startsWith("application/") and type.find("+json") }: return json.parse(body.readString)
        { type.startsWith("text/json") }: return json.parse(body.readString)
        { type.startsWith("text/") and type.find("+json") }: return json.parse(body.readString)
        { type.startsWith("text/") }: return body.readString
        { type.startsWith("application/javascript") }: return body.readString
        { type.startsWith("application/ecmascript") }: return body.readString
        { type.startsWith("application/") and type.find("xml") }: return body.readString
        { }: null
    body.read

HttpConnection = conn ->
    _Mutable_new {
        _req = null,
        _in = Buffer.new
        _length = 0

        _didhead = false,
        _didend = false
        _body = Buffer.new

        status = 200
        contentType = "text/plain"
        setContentType = type ->
            if this._didhead: throw "Already send headers"
            this.contentType = type

        req = ->
            if this._req: return this._req

            // TODO now we can just parse without "finding" first ...
            at = conn.find("\r\n\r\n", 0, MAX_HEADERS)
            if not at:
                if conn.size >= MAX_HEADERS: Error("Headers too long").throw
                Error("No headers received").throw
            headers = conn.readString(at + 4 - 1)
            this._req = httpparser.request(headers)

        method = -> this.req.method
        url = -> this.req.url
        headers = -> this.req.headers
        body = -> bodyFromConnection(conn, this.req.headers, args["stream"], this.method == "POST")

        error = ->
            this.status = 500
            this._didend = true
            this._body.clear
            this._body.write("500 internal error")
            this.flush(true)
            conn.close

        end = ->
            this._didend = true
            this.flush(true)
            // TODO don't always close
            conn.close

        write = ->
            if this._didend: Error("Already ended response").throw
            if this._didhead: conn.write(args)
            this._body.write(args)

        flush = ending ->
            if not this._didhead: this._sendheaders(ending)
            conn.write(this._body)
            conn.flush

        _sendheaders = ending ->
            this._didhead = true
            conn.buffer
            conn.write("HTTP/1.0 $(this.status) Ok\r\n")
            if this.contentType: conn.write("Content-Type: $(this.contentType)\r\n")
            if ending: conn.write("Content-Length: $(this._body.size)\r\n")
            conn.write("\r\n")
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
                    catch: e -> print e, e?msg; conn.error
                    block(conn); conn.end
                )
                Task.yield
        )
    }
}

openstream = url ->
    if url.type == "https":
        return ssl.Stream.new(io.Socket.open(url.host, url.port or 443))
    return io.Socket.open(url.host, url.port or 80)

// TODO take back the stream after body is done ...
send = method, url, body ->
    url = httpparser.url(url)
    conn = openstream(url)
    conn.buffer
    conn.write(method + " " + url.request + " HTTP/1.0\r\n")
    conn.write("Host: " + url.host + "\r\n")
    if args["userAgent"]: conn.write("User-Agent: $(args["userAgent"])\r\n")
    if args["userAgent"] == null: conn.write("User-Agent: Mozilla/5.0\r\n")
    if args["contentType"]: conn.write("Content-Type: $(args["contentType"])\r\n")
    if args["accept"]: conn.write("Accept: $(args["accept"])\r\n")
    body = { not body       }: body
           { isBuffer(body) }: body
           {                }: Buffer.new(body)
    if body: conn.write("Content-Length: $(body.size)\r\n")
    conn.write("\r\n")
    conn.write(body)
    conn.flush

    // TODO now we can just parse without "finding" first ...
    at = conn.find("\r\n\r\n", 0, MAX_HEADERS)
    if not at:
        if conn.size >= MAX_HEADERS: Error("Headers too long").throw
        Error("No headers received").throw
    header = conn.readString(at + 4 - 1)
    res = httpparser.response(header)
    body = bodyFromConnection(conn, res.headers, args["stream"])
    return body, res

get = url ->
    send("GET", url)

post = url, data ->
    send("POST", url, data)

{{ Server, get, post, send }}

