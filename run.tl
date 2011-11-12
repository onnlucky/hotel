if = cond, block -> bool(cond, block, (->))()
continue = -> _throw #continue
break = -> _throw #break

_Dir_open("test").each: (case ->
    if not case.endsWith ".tl": continue
    if case.endsWith "var.tl": break
    print "TESTCASE:", case
)

