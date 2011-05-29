%new = ( name, &call => %task_new(name, call) )
if = ( cond, &tb, &fb => bool(cond, tb, fb)() )

Variable = ( v =>
    loop = ( v =>
        from, msg, nv = %task_get()
        if(msg == #get, (%task_reply(from, v); goto(loop(v))))
        if(msg == #set, (%task_reply(from, nv); goto(loop(nv))))
        out("ERROR: unknown message: ", msg, "\n")
        %task_reply(from, null)
        goto(loop(v))
    )
    %new("variable", loop(v))
);

x = Variable(0)

out("get: ", %task_send(x, #get), "\n")
out("set: ", %task_send(x, #set, 100), "\n")
out("set: ", %task_send(x, #set, 200), "\n")
out("set: ", %task_send(x, #set, 300), "\n")
out("get: ", %task_send(x, #get), "\n")
out("get: ", %task_send(x, #get), "\n")
