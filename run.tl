_Dir_open("test").each: (file ->
    if not file.endsWith(".tl"): continue
    print "running $file"
    res, r = io.Process.run "./tl test/$(file)", -error
    if r != 0: print "FAIL: $file"
)

