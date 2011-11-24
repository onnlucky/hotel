// sketch of http server
if = cond, block -> bool(cond, block, (->))()
while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

// should be mutable object, not contain variables ... do'h
HttpConnection = (conn ->
    var $req = null
    in = _Buffer_new()
    var $sendheaders = false
    body = _Buffer_new()
    var $end = false
    {
        url: (->
            // read and parse until headers are complete
            while not $req: (
                if in.canread > 1024: _throw "BAD REQUEST"
                len = conn.read in
                if len == 0: _throw "IO ERROR"
                $req = { url: in.read }
                //ws = buf.find " "
                //method = buf.read ws
                //ws = buf.find " "
                //url = buf.read ws
                //$req {{ method, url }}
            )
            return $req.url
        )
        end: -> if not $end: $end = true; this.flush(true); conn.close
        write: (text ->
            if $end: _throw "Already Ended Response"
            body.write text
        )
        flush: (ending ->
            if not $sendheaders: this.sendheaders ending
            while body.canread > 0: (
                len = conn.write body
                if len == 0: _throw "IO ERROR"
            )
        )
        sendheaders: (ending ->
            $sendheaders = true
            headers = _Buffer_new()
            headers.write "HTTP/1.0 200 Ok\r\n"
            if ending: headers.write "Content-Length: $(body.canread)\r\n"
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

