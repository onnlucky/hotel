server = io.Socket.udp(0) # want to bind a port, just not any specify port
port = server.port
bg = !(
    catch: e -> log.error(e)
    data, ip, port = server.receive
    assert data.readString == "hello world!"
)

catch: e ->
    log.error(e)
    server?close
    bg?abort
    throw(e)

sender = io.Socket.udp()
sender.send("localhost", port, Buffer.new("hello world!"))
sleep(0.1)
assert bg.isDone
