http.Server.new(8080).serve: (conn ->
    print conn.req.url.path
    conn.write "HELLO WORLD!"
)

