// sketch of a repl

other = !(
    print 1
    Task.yield
    print 2
    Task.yield
    print 3
    Task.yield
    print 4
    Task.yield
    print 5
    Task.yield
    print "DONE"
)
print "other:", other
Task.yield
other.wait

//other.wait

/*
kq = kqueue()
eq = MsgQueue.new()
loop = !(
    Task.bindToNewThread
    loop:
        event = kevent(q)
        event.msg.reply
        if event.msg.name == #exit: return
)
!loop:
    msg = eq.get
    | msg.name == #canread ->
        kfeed(kq, msg[0], EVFILT_READ, msg)
)
fd = _File_open()
eq.input.canread fd

loop.wait

/*
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

