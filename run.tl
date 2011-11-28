#!/usr/bin/env tl
// ---- this should be in libraries ----

if = cond, block -> bool(cond, block, (->))()

while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

Time = { now: 42 }

seconds = ms -> ms * 1000
minutes = ms -> 60 * ms * 1000

// TODO extend Text.class with this ...
_text_split = (text, on ->
    onlen = on.size
    loop = (start, at, list->
        if text.startsWith(on, at): (
            goto(loop(at + onlen, at + onlen, list.add(text.slice(start, at))))
        )
        if text.size <= at: (
            return list.add(text.slice(start, at))
        )
        goto(loop(start, at + 1, list))
    )
    return loop(0, 0, [])
)

// ---- os ----

// TODO this is crap, should be per task and inherited or something ...
chdir = dir -> _io_chdir dir

run = (cmd ->
    s = _text_split(cmd, " ")
    // TODO add -in, +join
    child = _Child_run.call(s)
    child.in.close
    buf = _Buffer_new()
    while child.running: (
        child.out.read buf
    )
    child.err.read buf // TODO remove if +join works
    if child.status != 0: _throw buf.read
)


// ---- http ----

HttpConnection = (conn ->
    _Object_new {
        _req: null, _didhead: false, _didend: false
        _type: "text/text"
        _in: _Buffer_new()
        _body: _Buffer_new()
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
            headers = _Buffer_new()
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


// ---- this is the real example ----

var $state = { status: "init", time: Time.now }

!HttpServer.new 8080: (conn ->
    // TODO conn.contentType = "text/html"
    conn.setContentType "text/html"
    conn.write "<li>state: $( $state.status )<li>updated: $( $state.time )"
)

src = "tlbuilder"
repo = "onnlucky.com:hotel.git"

print "builder: $repo into $src"
run "rm -rf $src"
run "git clone $repo $src"
chdir src
while true: (
    (
        // TODO should be (...) catch: (...)
        _catch: (e->
            print "Error:", e
            $state = { status: e, time: Time.now }
        )
        print "cleaning and pulling ..."
        run "git reset --hard"
        run "git clean -xfd"
        run "git pull --force"
        print "building ..."
        run "make"
        print "testing ..."
        run "./tl test/fib.tl"
        print "ok!"
        $state = { status: "success", time: Time.now }
    )
    sleep minutes 5
)

