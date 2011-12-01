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

// io "main" loop
iocomands = TaskQueue.new
loop:
    loop:
        args = add.peek
        (
            match(not args, (-> break))
            match(args.msg == #read, (-> ev.waitread task.get(0); task.reply))
            match(args.msg == #write, (-> ev.waitwrite task.get(0); task.reply))
            match(args.msg == #wait, (-> ev.wait task.get(0); task.reply))
            match(true, (->args.throw "error"))
            throw "match error"
        )
    ev.run

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

