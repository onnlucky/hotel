# hotel http library; this is a bit of a toy library

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

# TODO read text encoding and not assume utf8
# TODO allow more control over decoding of type
bodyFromConnection = conn, headers, stream, expect ->
    length = headers["Content-Length"]
    transfer = headers["Transfer-Encoding"]
    encoding = headers["Content-Encoding"]
    type = headers["Content-Type"]
    body = { length                }: SizedStream(conn, length.eval)
           { transfer == "chunked" }: ChuckedStream(conn)
           { transfer              }: assert false, "unknown transfer encoding", transfer
           { not expect            }: Buffer.new
           {                       }: conn
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

WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
websocket_hash = key -> digest = _base64encode(openssl.sha1(key + WS_MAGIC))

# TODO make more like a stream
# TODO implement opcodes, sending with masks, sending long messages, ...
WebSocket = { this.socket, req ->
    # TODO and need to add this to not confuse parser/compiler
    x = (
        # in its own scope ... so locals don't become fields
        key = req.headers["Sec-WebSocket-Key"]
        if not key: Error("no Sec-WebSocket-Key header found; cannot upgrade").throw
        hash = websocket_hash(key)

        this.socket.write(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: $hash\r\n"
            "\r\n"
        )
    )

    read = ->
        opcode = this.socket.read(1)[1]
        masklen = this.socket.read(1)[1]
        len = masklen & 0b0111_1111
        length = { len == 127 }: this.socket.read(8)
                 { len == 126 }: this.socket.read(2)
                 {            }: len
        mask =   { masklen & 0b1000_0000 == 0 }: null
                 {                            }: this.socket.read(4)

        encoded = this.socket.read(length)
        #log.info("websocket<<", length, repr(mask), encoded.size)
        decoded = { mask }: Bin(encoded.map(b, n -> b ^ mask[1 + (n - 1) % 4])).toString
                  {      }: encoded.toString
        assert decoded
        return decoded

    write = msg ->
        assert msg.size < 126
        #log.info("websocket>>", msg)
        this.socket.write(129, msg.size, msg)
}

HttpConnection = conn ->
    _Mutable_new {
        port = -> conn.port
        ip = -> conn.ip
        local_port = -> conn.local_port
        local_ip = -> conn.local_ip

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

            # TODO now we can just parse without "finding" first ...
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

        upgrade = to ->
            if this._didhead: Error("Already send headers").throw
            this._didhead = true
            this._didend = true
            to(conn, this.req)

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
            # TODO don't always close
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
    new = port -> { sock = io.Socket.listen(port, host=args.get("host", "::")), class = this.class }
    class = {
        port = -> this.sock.port
        ip = -> this.sock.ip
        close = -> this.sock.close
        serve = ( ->
            block = args.block; if not block: throw "expect a block"
            loop:
                client = this.sock.accept
                if not client: return
                conn = HttpConnection(client)
                !(
                    catch: e -> log.error(e); conn.error
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

# TODO take back the stream after body is done ...
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

    # TODO now we can just parse without "finding" first ...
    at = conn.find("\r\n\r\n", 0, MAX_HEADERS)
    if not at:
        if conn.size >= MAX_HEADERS: Error("Headers too long").throw
        Error("No headers received").throw
    header = conn.readString(at + 4 - 1)
    res = httpparser.response(header)
    body = bodyFromConnection(conn, res.headers, args["stream"], method != "HEAD")
    return body, res

get = url ->
    send("GET", url)

post = url, data ->
    send("POST", url, data)

listen = port -> Server.new(port)
{{ Server, WebSocket, listen, get, post, send }}

