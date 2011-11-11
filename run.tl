if = cond, block -> bool(cond, block, (->))()
variable = !(
    loop = (v ->
        msg = Task.receive
        print("MSG:", msg)
        if msg.get(0) == #get: msg.reply v; goto loop v
        if msg.get(0) == #set: msg.reply msg.get(1); goto loop msg.get(1)
        print("ERROR: unknown message:", msg)
        msg.reply
        goto loop v
    )
    loop()
)
variable.send #set, 42
print variable.send #get

/*
Latch = new {
    running: 0
    waiters: List.new
    start: (->
        @running += 1
        if running <= 5: return
        @waiters.add Task.current
        Task.current.park
    )
    done: (->
        @running -= 1
        @waiters.pop.unpark
    )
}


