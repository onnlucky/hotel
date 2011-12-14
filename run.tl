io.Socket.listen(9999).serve: (conn ->
    conn.write "hello world!"
)

/*
http.Server.new(8080).serve: (conn ->
    conn.write "hello world!"
)

