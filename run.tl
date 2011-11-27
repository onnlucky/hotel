// sketch of http server
if = cond, block -> bool(cond, block, (->))()
while = (&cond, block ->
    loop = -> if cond(): block(); goto loop()
    loop()
)

stdin = _File_from(0)
buf = _Buffer_new()
while true: (
    out "> "
    stdin.read buf
    _catch: e -> out("error: $e\n")
    print eval(buf.read)
)

