// should be in boot or build into vm
if = cond, block -> bool(cond, block, (->))()
continue = -> _throw #continue
break = -> _throw #break

// allow upto max tasks running concurrently by holding off a reply to their messages
Latch = (max ->
    guard = !(
        loop = (count, hold ->
            msg = Task.receive
            if msg.get(0) == #enter: (
                if count < max: msg.reply; goto loop(count + 1, hold)
                goto loop(count + 1, hold.add(msg))
            )
            if msg.get(0) == #exit: (
                msg.reply
                if hold.size == 0: goto loop(count - 1, hold)
                hold.get(0).reply
                goto loop(count - 1, hold.slice(1))
            )
            print "ERROR"; msg.reply; loop(count, hold)
        )
        loop(0, [])
    )
    (block -> guard.send #enter; block(); guard.send #exit)
)

// actual tester
var $fail = 0
var $pass = 0

latch = Latch(4)

_Dir_open("test").each: (case ->
    if not case.endsWith ".tl": continue
    !latch: (
        name = case.slice(0, -3)
        test = _Child_run "/bin/sh", "-c", "cd test; ../tl $case 2>/dev/null | diff $name.ref -"
        status = test.wait
        if status != 0: (
            buf = _Buffer_new()
            test.out.read(buf)
            print "---- $name -- ERROR ----:\n".cat(buf.read).cat("----")
            $fail = $fail + 1
        )
        if status == 0: (
            print "PASS $name"
            $pass = $pass + 1
        )
    )
)
print "pass:", $pass, "fail:", $fail

