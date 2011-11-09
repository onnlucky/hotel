if = cond, &tb, &fb -> bool(cond, tb, fb)()

Variable = ( v ->
    _task_new((->
        loop = ( v ->
            from, msg, nv = _task_receive()
            if(msg == #get, (_task_reply(from, v); goto(loop(v))))
            if(msg == #set, (_task_reply(from, nv); goto(loop(nv))))
            out("ERROR: unknown message: ", msg, "\n")
            _task_reply(from, null)
            goto(loop(v))
        )
        loop(v)
    ))
)

x = Variable(0)
out("get: ", _task_send(x, #get), "\n")
out("set: ", _task_send(x, #set, 100), "\n")
out("set: ", _task_send(x, #set, 200), "\n")
out("set: ", _task_send(x, #set, 300), "\n")
out("get: ", _task_send(x, #get), "\n")
out("get: ", _task_send(x, #get), "\n")
