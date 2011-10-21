if = { c, block -> bool(c, block, {})() }

dir = _Dir_open("test")
dir.each: { p ->
    if (p.endsWith(".tl")): {
        result = _Child_run("./tl", "test/".cat(p)).wait
        print "TEST:", p, "RESULT:", result
    }
}

