buf = _Buffer_new()
buf.write "GET /a?/really/long/path.html HTTP/1.0\r\nHOST: foobarfoobar.com\r\n\r\n"
print _http_parse(buf)
print "DONE"

