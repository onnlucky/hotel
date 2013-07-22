if = cond, block -> bool(cond, block, (->))()

Latch = max ->
    queue = MsgQueue.new
    !(
        loop = count, hold ->
            msg = queue.get
            if msg.name == #enter:
                if count < max: msg.reply; goto loop(count + 1, hold)
                goto loop(count + 1, hold.add(msg))
            if msg.name == #exit:
                print "EXITING:", hold.size
                msg.reply
                if hold.size == 0: goto loop(count - 1, hold)
                hold[1].reply
                goto loop(count - 1, hold[2:])
            print "********ERROR*********"
            msg.reply; loop(count, hold)
        loop(0, [])
    )
    {
        enter: -> queue.input.enter
        exit: -> queue.input.exit
    }

latch = Latch(2)

doTheFoo = ->
    //print "STARTING..."
    //var $count = random(10)
    //loop: Task.yield; $count -= 1; if $count <= 0: break
    //print "ENTERING..."
    latch.enter
    //print "WORKING..."
    //var $count = random(100)
    //loop: Task.yield; $count -= 1; if $count <= 0: break
    //print "WORK ALMOST DONE..."
    latch.exit
    //print "DONE..."

!doTheFoo()
!doTheFoo()
!doTheFoo()
!doTheFoo()
!doTheFoo()
!doTheFoo()
!doTheFoo()
Task.yield
doTheFoo()
Task.yield

return null
