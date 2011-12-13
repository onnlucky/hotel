!io.Socket.listen 8080: (conn ->
    print conn.read.toText
    conn.close
)

stream = io.Socket.open("www.google.com", 80)
stream.write "GET / HTTP/1.0\r\n\r\n"
print stream.read.toText

print io.File("run.tl").read
io.File("test.txt").write "HELLO WORLD!"
io.File("test.txt").read

