io.Socket.listen(9999).serve: (conn ->
    conn.write "HTTP/1.0 200 Ok\r\n\r\n"
    conn.write "hello world!"
)

http.Server.new(9999).serve: (conn ->
    conn.write "hello world!"
)

