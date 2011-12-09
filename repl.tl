// hotel repl
// TODO a bit primitive ... but works .. use higher level io lib, and return function?

reader = _File_from(0).reader
buf = _Buffer_new()
var $env = Env.new(Env.current)
while true: (
    out "> "
    _with_lock(buf, reader): (
        loop: (
            len = reader.read buf
            if len: break
            if len == null: _io.waitread reader
        )
    )
    _catch: e -> out("error: $e\n")
    res = eval(env, buf.read)
    if res: print res
)

