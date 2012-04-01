file = _File_open("readfile.tl", _File_RDONLY)
buf = _Buffer_new()
len = file.read buf
file.close
out("len: ", len, "\n")
out(buf.read)

