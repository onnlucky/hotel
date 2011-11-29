// sketch of a repl
if = cond, block -> bool(cond, block, (->))()
while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

stdin = _File_from(0)
buf = _Buffer_new()
var $env = Env.new(Env.current)
while true: (
    out "> "
    stdin.read buf
    _catch: e -> out("error: $e\n")
    // eval wants a var with an env inside, a pointer, basically
    res = eval(env, buf.read)
    if res: print res
)

