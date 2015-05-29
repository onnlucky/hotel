server = http.Server.new()
port = server.port
bg = !(
    catch: e -> log.error(e)
    server.serve: conn ->
        assert conn.method == "GET" or conn.method == "POST"
        conn.write("thanks")
)

catch: e ->
    log.error(e)
    server?close
    bg?abort
    throw(e)

assert http.get("http://localhost:$port") == "thanks"
assert http.post("http://localhost:$port", "hello world!") == "thanks"
server.close
