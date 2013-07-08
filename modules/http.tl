// hotel http library

HttpConnection = conn ->
    _Object_new {
        _req: null, _didhead: false, _didend: false
        _in: io.Buffer.new
        _body: io.Buffer.new
        status: 200
        contentType: "text/plain"
        setContentType: (type ->
            if @_didhead: throw "Already send headers"
            @contentType = type
        )
        req: (->
            // read and parse until headers are complete
            if @_req: return @_req
            loop:
                if @_in.size > 2048: print @_in.read; conn.close; throw "REQUEST TOO LONG"
                if @_in.find("\r\n\r\n"): break
                if @_in.find("\n\n"): break
                _, len = conn.read @_in
                if len == 0: throw "IO ERROR"
            @_req = httpparser.request(@_in.read)
        )
        end: -> @_didend = true; @flush(true); conn.close
        error: (->
            @status = 500
            @_didend = true
            @_body.clear; @_body.write("500 internal error")
            @flush(true); conn.close
        )
        write: (text ->
            if @_didend: throw "Already Ended Response"
            @_body.write text
        )
        flush: (ending ->
            if not @_didhead: @_sendheaders(ending)
            while @_body.size > 0:
                len = conn.write @_body
                //if len == 0: throw "IO ERROR"
        )
        _sendheaders: (ending ->
            @_didhead = true
            headers = io.Buffer.new()
            headers.write "HTTP/1.0 $(@status) Ok\r\n"
            if @contentType: headers.write "Content-Type: $(@contentType)\r\n"
            if ending: headers.write "Content-Length: $(@_body.size)\r\n"
            headers.write "\r\n"
            while headers.size > 0:
                len = conn.write headers
                //if len == 0: throw "IO ERROR"
        )
    }

Server = {
    new: port -> { sock: io.Socket.listen(port), class: this.class }
    class: {
        port: -> this.sock.port
        close: -> this.sock.close
        serve: ( ->
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

    // TODO implement encodings and taking back the stream after body is done ...
    res = httpparser.response(conn.read(at - 1 + 4))
    if args.names.stream:
        return conn, res

    length = res.headers.get("Content-Length")
    if length:
        length = length.eval
        body = conn.read length
        if not body or body.size < length:
            Error("Incomplete body received").throw
        return body, res
    return conn.read, res

return { Server: Server, get: get }

