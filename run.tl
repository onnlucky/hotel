if = cond, block -> bool(cond, block, (->))()
continue = -> _throw #continue
break = -> _throw #break

var $fail = 0
var $pass = 0

_Dir_open("test").each: (case ->
    if not case.endsWith ".tl": continue
    test = case.slice(0, -3)
    cmd = "cd test; ../tl ".cat(case).cat(" 2>/dev/null | diff ").cat(test).cat(".ref -")

    print "RUNNING:", test
    test = _Child_run("/bin/sh", "-c", cmd)
    status = test.wait
    if status != 0: (
        buf = _Buffer_new()
        test.out.read(buf)
        print "---- ERROR ----:\n".cat(buf.read).cat("----")
        $fail = $fail + 1
    )
    if status == 0: $pass = $pass + 1
)
print "pass:", $pass, "fail:", $fail

