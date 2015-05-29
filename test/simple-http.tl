var $hits = 0

server = http.Server.new(18080)
bg = !(
    catch: e -> log.error(e)
    server.serve: client ->
        $hits += 1
        client.write("hello world!")
)

# ensure failing tests still close server ...
catch: e ->
    log.error(e)
    server?close
    bg?abort
    throw(e)

out, status = io.run("curl -s http://localhost:18080/")
assert status == 0
assert out == "hello world!"

out, status = io.run("curl -s http://localhost:18080/")
assert status == 0
assert out == "hello world!"

server.close

assert $hits == 2

