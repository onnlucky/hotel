if = cond, block -> bool(cond, block, (->))()

_Dir_open("test").each(foo=42): (case ->
    if case.endsWith ".tl": (
        print "TEST:", case
    )
)

