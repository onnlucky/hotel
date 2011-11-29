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
    len = stdin.read buf
    if len == 0: return null

    _catch: e -> out("error: $e\n")
    res = eval(env, buf.read)
    if res: print res
)

