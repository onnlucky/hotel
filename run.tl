// sketch of http server
if = cond, block -> bool(cond, block, (->))()
while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

HttpConnection = (conn ->
    _Object_new {
        _req: null, _didhead: false, _didend: false
        _in: _Buffer_new()
        _body: _Buffer_new()
        url: (->
            // read and parse until headers are complete
            while not @_req: (
                if @_in.canread > 1024: _throw "BAD REQUEST"
                len = conn.read @_in
                if len == 0: _throw "IO ERROR"
                @_req = { url: @_in.read }
                //ws = buf.find " "
                //method = buf.read ws
                //ws = buf.find " "
                //url = buf.read ws
                //@_req {{ method, url }}
            )
            return @_req.url
        )
        end: (-> if not @_didend: @_didend = true; @flush(true); conn.close)
        write: (text ->
            if @_didend: _throw "Already Ended Response"
            @_body.write text
        )
        flush: (ending ->
            if not @_didhead: @_sendheaders(ending)
            while @_body.canread > 0: (
                len = conn.write @_body
                if len == 0: _throw "IO ERROR"
            )
        )
        _sendheaders: (ending ->
            @_didhead = true
            headers = _Buffer_new()
            headers.write "HTTP/1.0 200 Ok\r\n"
            if ending: headers.write "Content-Length: $(@_body.canread)\r\n"
            headers.write "\r\n"
            while headers.canread > 0: (
                len = conn.write headers
                if len == 0: _throw "IO ERROR"
            )
        )
    }
)

HttpServer = {
    new: (port, block ->
        socket = _ServerSocket_listen(8080)
        while true: (
            client = socket.accept
            conn = HttpConnection(client)
            !block(conn); conn.end
        )
    )
}

var $count = 0
HttpServer.new(8080): (conn->
    conn.write "hello from Hotel!\nyou are number: $( $count += 1 )"
)

