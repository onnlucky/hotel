if = cond, block -> bool(cond, block, (->))()

Latch = (max ->
    guard = !(
        loop = (count, hold ->
            msg = Task.receive
            if msg.get(0) == #enter: (
                if count < max: msg.reply; goto loop(count + 1, hold)
                goto loop(count + 1, hold.add(msg))
            )
            if msg.get(0) == #exit: (
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
        enter: -> guard.send #enter
        exit: -> guard.send #exit
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
