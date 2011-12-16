Counter = _Object_new {
    count: 0
    hit: -> @count = @count + 1
}

http.Server.new(9999).serve: (conn ->
    nr = Counter.hit
    print "REQ: $(nr)", conn.req.size
    //sleep seconds 1
    conn.setContentType("text/html")
    conn.write "<html>\n<head><title>YO!</title></head>\n<body>"
    conn.write "<p>Hi there; you are number: $(nr); thanks!</p>"
    conn.write "</body>\n</html>\n"
    print "DONE: $(nr)"
)

