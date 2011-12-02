// test buildins


//| false -> print "ERROR"
//| true -> print "TRUE"
// compiles into:
(
  _match(false): print "ERROR"
  _match(true): print "OK"
  _nomatch
)

if true: print "OK 1"
if false: print "ERROR"
if false: print "ERROR"
if true: print "OK 2"

loop: (
    print "OK 3"
    break
    print "ERROR"
)

while = (&cond, block ->
    loop: (
        if not cond(): break
        block()
    )
)

while true: (
    print "OK 4"
    break
    print "ERROR"
)
print "done"

