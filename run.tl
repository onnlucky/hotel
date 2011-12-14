io.Socket.listen(8080).serve: (conn ->
    conn.write "hello world!"
)

/*
http.Server.new(8080).serve: (conn ->
    conn.write "hello world!"
)

