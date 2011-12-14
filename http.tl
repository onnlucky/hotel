// hotel http library

HttpConnection = (conn ->
    _Object_new {
        _req: null, _didhead: false, _didend: false
        _type: "text/plain"
        _in: io.Buffer.new
        _body: io.Buffer.new
        setContentType: (type ->
            if @_didhead: _throw "Already send headers"
            @_type = type
        )
        req: (->
            // read and parse until headers are complete
            while not @_req: (
                if @_in.canread > 1024: _throw "BAD REQUEST"
                len = conn.read @_in
                if len == 0: _throw "IO ERROR"
                @_req = { url: @_in.read }
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
            headers = io.Buffer.new()
            headers.write "HTTP/1.0 200 Ok\r\n"
            if @_type: headers.write "Content-Type: $(@_type)\r\n"
            if ending: headers.write "Content-Length: $(@_body.canread)\r\n"
            headers.write "\r\n"
            while headers.canread > 0: (
                len = conn.write headers
                if len == 0: _throw "IO ERROR"
            )
        )
    }
)

Server = {
    new: (port -> { sock: io.Socket.listen(port), class: this.class })
    class: {
        close: -> this.sock.close
        serve: (->
            block = args.block; if not block: _throw "expect a block"
            loop: (
                client = this.sock.accept
                conn = HttpConnection(client)
                block(conn); conn.end
            )
        )
    }
}

return { Server: Server }

