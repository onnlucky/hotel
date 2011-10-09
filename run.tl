file = _File_open("TEST-WRITE-FILE", _File_RDWR + _File_TRUNC + _File_CREAT)
buf = _Buffer_new();
buf.write "hello world!\n"
file.write buf

