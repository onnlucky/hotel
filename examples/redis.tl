launch = { dir ->
    print "starting redis"
    io.run("redis-server --daemonize yes --pidfile $dir/pid --unixsocket $dir/socket --port 0")
}

connect = { dir ->
    path = io.Path(dir)
    if not path.exists: path.create
    if not path.isDir: _throw "not a dir"

    pid = "$dir/pid"
    if not io.Path(pid).exists: launch(dir)

    io.Stream.new(_Socket_connect_unix("$dir/socket"))
}

conn = connect("test")
conn.write("*1\r\n\$4\r\nPING\r\n")
print conn.read.toString

