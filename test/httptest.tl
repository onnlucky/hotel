server = http.Server.new(18888)
!server.serve: conn ->
    assert conn.method == "GET" or conn.method == "POST"
    conn.write("thanks")
assert http.get("http://localhost:18888") == "thanks"
assert http.post("http://localhost:18888", "hello world!") == "thanks"
server.close
