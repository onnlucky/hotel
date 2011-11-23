if = cond, block -> bool(cond, block, (->))()

Latch = (max ->
    queue = MsgQueue.new
    !(
        loop = (count, hold ->
            msg = queue.get
            if msg.name == #enter: (
                if count < max: msg.reply; goto loop(count + 1, hold)
                goto loop(count + 1, hold.add(msg))
            )
            if msg.name == #exit: (
                print "EXITING:", hold.size
                msg.reply
                if hold.size == 0: goto loop(count - 1, hold)
                hold.get(0).reply
                goto loop(count - 1, hold.slice(1))
            )
            print "********ERROR*********"
            msg.reply; loop(count, hold)
        )
        loop(0, [])
    )
    {
        enter: -> queue.input.enter
        exit: -> queue.input.exit
    }
)

latch = Latch(2)

doTheFoo = (->
    //print "STARTING..."
    sleep random(10)
    //print "ENTERING..."
    latch.enter
    //print "WORKING..."
    sleep random(100)
    //print "WORK ALMOST DONE..."
    latch.exit
    //print "DONE..."
)

!doTheFoo
!doTheFoo
!doTheFoo
!doTheFoo
!doTheFoo
!doTheFoo
!doTheFoo
!doTheFoo

return null
