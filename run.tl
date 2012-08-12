//client = http.Client.new
//client.get("http://onnlucky.com")

s = io.Socket.open("onnlucky.com", 80)
s.write("GET / HTTP/1.0\r\nHost: onnlucky.com\r\n\r\n")
print s.read

