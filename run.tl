//io.Socket.listen(9999).serve: (conn ->
//    conn.write "hello world!"
//)

http.Server.new(9999).serve: (conn ->
    print "CONNECTION"
    conn.write "hello world!"
)

