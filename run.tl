server = http.Server.new(8080)
!server.serve: { client ->
    client.write "hello"
}

print "WAITING"
io.wait 1000
print "CLOSING"
server.close
print "DONE"

