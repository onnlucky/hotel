if = cond, block -> bool(cond, block, (->))()

Variable = (v ->
    !(
        loop = (v ->
            msg = Task.receive
            if msg.get(0) == #get: msg.reply v; goto loop v
            if msg.get(0) == #set: msg.reply msg.get(1); goto loop msg.get(1)
            print("ERROR: unknown message:", msg.get(0))
            msg.reply
            goto loop v
        )
        loop(v)
    )
)

x = Variable(0)
out("get: ", x.send(#get), "\n")
out("set: ", x.send(#set, 100), "\n")
out("set: ", x.send(#set, 200), "\n")
out("set: ", x.send(#set, 300), "\n")
out("get: ", x.send(#get), "\n")
out("get: ", x.send(#get), "\n")

