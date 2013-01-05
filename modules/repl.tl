// hotel repl
// TODO a bit primitive ... but works .. use higher level io lib, and return function?

_env = Env.current

/*
run = (stream ->
    buf = _Buffer_new()
    var $env = Env.new(_env)
    loop: (
        stream.write "> "
        input = stream.read
        catch: e -> stream.write("Error: $e\n")
        res = eval(env, input)
        if res: print res
    )
)
*/

// TODO this should be a stream and called ...
reader = _File_from(0).reader
run = { ->
    buf = _Buffer_new()
    var $env = Env.new(_env)
    loop: {
        out "> "
        _with_lock(buf, reader): {
            loop: {
                len = reader.read buf
                if len == 0: return
                if len: break
                if len == null: _io.waitread reader
            }
        }
        catch: e -> out("error: $e\n")
        res = eval(env, buf.read)
        if res: print res
    }
}

{ run: run }

