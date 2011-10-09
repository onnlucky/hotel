domany = ->{
    file = _File_open("/etc/passwd", _File_RDONLY)
    buf = _Buffer_new();
    len = file.read buf
    print "read:", len
}

domany
domany
domany

