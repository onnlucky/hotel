if = cond, block -> bool(cond, block, (->))()

Variable = (v ->
    queue = MsgQueue.new
    !(
        loop = (v ->
            msg = queue.get
            if msg.name == #get: msg.reply v; goto loop v
            if msg.name == #set: msg.reply msg.get(0); goto loop msg.get(0)
            print("ERROR: unknown message:", msg.name)
            msg.reply
            goto loop v
        )
        loop(v)
    )
    return queue.input
)

x = Variable(0)
out("get: ", x.get, "\n")
out("set: ", x.set(100), "\n")
out("set: ", x.set(200), "\n")
out("set: ", x.set(300), "\n")
out("get: ", x.get, "\n")
out("get: ", x.get, "\n")

