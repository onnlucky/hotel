child = _Child_run("hello", "world")
child.wait
buf = _Buffer_new()
child.out.read(buf)
print buf.read

/*
print _Socket_resolve("www.google.com")
return

stat = _Path_stat("tl")
print stat.ino
print stat.mtime
return

dir = _Dir_open(".")
print dir.read
print dir.read
print dir.read
print dir.read
print dir.read
print dir.read
return

/*
server = _ServerSocket_listen(8080)
client = server.accept

buf = _Buffer_new()
client.read buf
client.write buf

/*
file = _File_open("TEST-WRITE-FILE", _File_RDWR + _File_TRUNC + _File_CREAT)
buf = _Buffer_new();
buf.write "hello world!\n"
file.write buf

sock = _Socket_connect("74.125.77.104", 80)
buf.write "GET / HTTP/1.0\r\n\r\n"
sock.write buf
sock.read buf
print buf.read

