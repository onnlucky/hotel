server = http.Server.new(18888)
!server.serve: conn -> conn.write("thanks")
assert http.get("http://localhost:18888") == "thanks"
server.close
