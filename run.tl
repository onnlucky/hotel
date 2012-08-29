//client = http.Client.new
//client.get("http://onnlucky.com")

s = io.Socket.open("www.google.nl", 443)
ssl = SSL.new

read = { s, into ->
    _with_lock(ssl, into): {
        loop: {
            len = ssl.read(into)
            if ssl.wbuf.size > 0: s.write ssl.wbuf
            if len > 0: return len
            s.read ssl.rbuf
        }
    }
}
write = { s, from ->
    _with_lock(ssl, from): {
        len = from.size
        loop: {
            mustread = ssl.write(from)
            s.write ssl.wbuf
            if mustread: {
                _, len = s.read ssl.rbuf
                print("read:", len)
                if len == 0: _throw("closed")
                continue;
            }
            if from.size == 0 and ssl.wbuf.size == 0: return len
        }
    }
}

b = Buffer.new
b.write "HEAD / HTTP/1.0\r\n\r\n"
write(s, b)
print("READING...")
read(s, b)
print b.read

