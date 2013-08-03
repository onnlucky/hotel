// hotel repl
// example: io.Socket.listen(2222).serve: c -> repl.run(c)

run = stream ->
    _env = Env.current
    var $env = Env.new(_env)
    while not stream.readClosed:
        stream.write "> "
        catch: e -> stream.write("error: $(e.toString)\n")
        nl = stream.find("\n")
        if not nl: return
        res = eval(env, stream.read(nl))
        if res: stream.write("$res\n")

{{ run }}

