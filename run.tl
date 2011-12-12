stream = io.Socket.open("www.google.com", 80)
stream.write "GET / HTTP/1.0\r\n\r\n"
stream.read.toText

/*
print io.File("run.tl").read
io.File("test.txt").write "HELLO WORLD!"
io.File("test.txt").read
