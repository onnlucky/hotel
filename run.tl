file = _File_open("/etc/passwd", _File_RDONLY)
buf = _Buffer_new();
file.read buf
buf.read

